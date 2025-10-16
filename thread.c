#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

typedef struct {
    int width;
    int height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep *row_pointers;
} PngImage;

// Thread data structure
typedef struct {
    int thread_id;
    int num_threads;
    int width;
    int height;
    int** work;
    unsigned char** output;
    // Synchronization structures for each pixel
    pthread_mutex_t*** pixel_mutexes;
    pthread_cond_t*** pixel_conditions;
    int*** pixel_processed;
} ThreadData;

// Function declarations (for cleaner structure)
PngImage* read_png_file(const char* filename);
void free_png_image(PngImage *image);
unsigned char rgb_to_grayscale(unsigned char r, unsigned char g, unsigned char b);
void write_png_file(const char* filename, unsigned char** data, int width, int height);
int floor_divide(int numerator, int denominator);
void* process_wavefront(void* arg);
void dither_image_mt(unsigned char** input, unsigned char** output, int width, int height, int num_threads);
void dither_image_st(unsigned char** input, unsigned char** output, int width, int height);


// ------------------------- PNG I/O and Utility Functions -------------------------

PngImage* read_png_file(const char* filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    PngImage *image = (PngImage*)malloc(sizeof(PngImage));
    image->width = png_get_image_width(png, info);
    image->height = png_get_image_height(png, info);
    image->color_type = png_get_color_type(png, info);
    image->bit_depth = png_get_bit_depth(png, info);

    if (image->bit_depth == 16) png_set_strip_16(png);
    if (image->color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (image->color_type == PNG_COLOR_TYPE_GRAY && image->bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    
    // Ensure 32-bit (RGBA) format for easy access (R, G, B, A)
    if (image->color_type == PNG_COLOR_TYPE_RGB ||
        image->color_type == PNG_COLOR_TYPE_GRAY ||
        image->color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (image->color_type == PNG_COLOR_TYPE_GRAY ||
        image->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    image->row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * image->height);
    for (int y = 0; y < image->height; y++) {
        image->row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, image->row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return image;
}

void free_png_image(PngImage *image) {
    if (image) {
        for (int y = 0; y < image->height; y++) {
            free(image->row_pointers[y]);
        }
        free(image->row_pointers);
        free(image);
    }
}

// don't change this function (rgb_to_grayscale)
unsigned char rgb_to_grayscale(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char result = (unsigned char)((0.2989 * r + 0.587 * g + 0.114 * b));
    if (result < 255 && result > 0) {
        result++;
    }
    return result;
}

void write_png_file(const char* filename, unsigned char** data, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_GRAY, 
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(width);
        for (int x = 0; x < width; x++) {
            row_pointers[y][x] = data[y][x];
        }
    }

    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
}

// Custom floor division function to match Python's //
int floor_divide(int numerator, int denominator) {
    if (numerator >= 0) {
        return numerator / denominator;
    } else {
        // For negative numbers, this matches Python's floor division
        return (numerator - denominator + 1) / denominator;
    }
}

// ------------------------- Multi-Threading Dithering Logic -------------------------

// Wavefront pattern with explicit diagonal synchronization
void* process_wavefront(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int width = data->width;
    int height = data->height;
    
    // Process diagonals in wavefront pattern
    for (int diag = 0; diag < width + height - 1; diag++) {
        // Each thread processes every num_threads-th diagonal
        if (diag % data->num_threads != data->thread_id) {
            continue;
        }
        
        // Process all pixels in this diagonal
        for (int y = 0; y < height; y++) {
            int x = diag - y;
            if (x >= 0 && x < width) {
                
                // --- 1. WAIT FOR DEPENDENCIES (Error from prior pixels must be written) ---
                
                // Wait for top-right neighbor (y-1, x+1)
                if (y > 0 && x + 1 < width) {
                    pthread_mutex_lock(data->pixel_mutexes[y-1][x+1]);
                    while (!(*data->pixel_processed[y-1][x+1])) {
                        pthread_cond_wait(data->pixel_conditions[y-1][x+1], data->pixel_mutexes[y-1][x+1]);
                    }
                    pthread_mutex_unlock(data->pixel_mutexes[y-1][x+1]);
                }
                
                // Wait for left neighbor (y, x-1)
                if (x > 0) {
                    pthread_mutex_lock(data->pixel_mutexes[y][x-1]);
                    while (!(*data->pixel_processed[y][x-1])) {
                        pthread_cond_wait(data->pixel_conditions[y][x-1], data->pixel_mutexes[y][x-1]);
                    }
                    pthread_mutex_unlock(data->pixel_mutexes[y][x-1]);
                }
                
                // --- 2. PROCESS THE PIXEL ---
                
                int old_pixel = data->work[y][x];
                int new_pixel = (old_pixel > 128) ? 255 : 0;
                data->output[y][x] = (unsigned char)new_pixel;
                int err = old_pixel - new_pixel;

                // --- 3. PROPAGATE ERROR (Requires Lock on Target Pixels to prevent race conditions) ---
                
                // (y, x + 1) -> 7/16
                if (x + 1 < width) {
                    pthread_mutex_lock(data->pixel_mutexes[y][x + 1]);
                    data->work[y][x + 1] += floor_divide(err * 7, 16);
                    pthread_mutex_unlock(data->pixel_mutexes[y][x + 1]);
                }
                
                if (y + 1 < height) {
                    // (y + 1, x - 1) -> 3/16
                    if (x - 1 >= 0) {
                        pthread_mutex_lock(data->pixel_mutexes[y + 1][x - 1]);
                        data->work[y + 1][x - 1] += floor_divide(err * 3, 16);
                        pthread_mutex_unlock(data->pixel_mutexes[y + 1][x - 1]);
                    }
                    
                    // (y + 1, x) -> 5/16
                    pthread_mutex_lock(data->pixel_mutexes[y + 1][x]);
                    data->work[y + 1][x] += floor_divide(err * 5, 16);
                    pthread_mutex_unlock(data->pixel_mutexes[y + 1][x]);
                    
                    // (y + 1, x + 1) -> 1/16
                    if (x + 1 < width) {
                        pthread_mutex_lock(data->pixel_mutexes[y + 1][x + 1]);
                        data->work[y + 1][x + 1] += floor_divide(err * 1, 16);
                        pthread_mutex_unlock(data->pixel_mutexes[y + 1][x + 1]);
                    }
                }
                
                // --- 4. SIGNAL COMPLETION ---
                
                // Mark current pixel as processed (Lock on CURRENT pixel)
                pthread_mutex_lock(data->pixel_mutexes[y][x]);
                *data->pixel_processed[y][x] = 1;
                pthread_cond_broadcast(data->pixel_conditions[y][x]);
                pthread_mutex_unlock(data->pixel_mutexes[y][x]);
            }
        }
    }
    
    return NULL;
}

// Multi-threaded dithering with diagonal dependencies
void dither_image_mt(unsigned char** input, unsigned char** output, int width, int height, int num_threads) {
    // Create working array
    int** work = (int**)malloc(height * sizeof(int*));
    for (int y = 0; y < height; y++) {
        work[y] = (int*)malloc(width * sizeof(int));
        for (int x = 0; x < width; x++) {
            work[y][x] = input[y][x];
        }
    }

    // Initialize synchronization structures for each pixel
    pthread_mutex_t*** pixel_mutexes = (pthread_mutex_t***)malloc(height * sizeof(pthread_mutex_t**));
    pthread_cond_t*** pixel_conditions = (pthread_cond_t***)malloc(height * sizeof(pthread_cond_t**));
    int*** pixel_processed = (int***)malloc(height * sizeof(int**));
    
    for (int y = 0; y < height; y++) {
        pixel_mutexes[y] = (pthread_mutex_t**)malloc(width * sizeof(pthread_mutex_t*));
        pixel_conditions[y] = (pthread_cond_t**)malloc(width * sizeof(pthread_cond_t*));
        pixel_processed[y] = (int**)malloc(width * sizeof(int*));
        
        for (int x = 0; x < width; x++) {
            pixel_mutexes[y][x] = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
            pixel_conditions[y][x] = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
            pixel_processed[y][x] = (int*)malloc(sizeof(int));
            
            pthread_mutex_init(pixel_mutexes[y][x], NULL);
            pthread_cond_init(pixel_conditions[y][x], NULL);
            *pixel_processed[y][x] = 0;
        }
    }

    // Create threads
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    ThreadData* thread_data = (ThreadData*)malloc(num_threads * sizeof(ThreadData));
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_threads = num_threads;
        thread_data[i].width = width;
        thread_data[i].height = height;
        thread_data[i].work = work;
        thread_data[i].output = output;
        thread_data[i].pixel_mutexes = pixel_mutexes;
        thread_data[i].pixel_conditions = pixel_conditions;
        thread_data[i].pixel_processed = pixel_processed;
        
        // Using the corrected wavefront processing function
        pthread_create(&threads[i], NULL, process_wavefront, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Cleanup
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            pthread_mutex_destroy(pixel_mutexes[y][x]);
            pthread_cond_destroy(pixel_conditions[y][x]);
            free(pixel_mutexes[y][x]);
            free(pixel_conditions[y][x]);
            free(pixel_processed[y][x]);
        }
        free(pixel_mutexes[y]);
        free(pixel_conditions[y]);
        free(pixel_processed[y]);
        free(work[y]);
    }
    free(pixel_mutexes);
    free(pixel_conditions);
    free(pixel_processed);
    free(work);
    free(threads);
    free(thread_data);
}

// Single-threaded version for comparison
void dither_image_st(unsigned char** input, unsigned char** output, int width, int height) {
    int** work = (int**)malloc(height * sizeof(int*));
    for (int y = 0; y < height; y++) {
        work[y] = (int*)malloc(width * sizeof(int));
        for (int x = 0; x < width; x++) {
            work[y][x] = input[y][x];
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int old_pixel = work[y][x];
            int new_pixel = (old_pixel > 128) ? 255 : 0;
            output[y][x] = (unsigned char)new_pixel;
            int err = old_pixel - new_pixel;

            if (x + 1 < width)  
                work[y][x + 1] += floor_divide(err * 7, 16);
            if (y + 1 < height) {
                if (x - 1 >= 0) 
                    work[y + 1][x - 1] += floor_divide(err * 3, 16);
                work[y + 1][x] += floor_divide(err * 5, 16);
                if (x + 1 < width)  
                    work[y + 1][x + 1] += floor_divide(err * 1, 16);
            }
        }
    }

    for (int y = 0; y < height; y++) {
        free(work[y]);
    }
    free(work);
}

// ------------------------- Main Function -------------------------

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        printf("Usage: %s <input.png> <output.png> [num_threads]\n", argv[0]);
        printf("Default: 1 thread\n");
        return 1;
    }

    const char* input_file = argv[1];
    const char* image_output = argv[2];
    int num_threads = (argc == 4) ? atoi(argv[3]) : 1;

    PngImage *image = read_png_file(input_file);
    if (!image) {
        printf("Error: Could not read %s\n", input_file);
        return 1;
    }

    // Allocate arrays
    unsigned char** grayscale = (unsigned char**)malloc(image->height * sizeof(unsigned char*));
    unsigned char** dithered = (unsigned char**)malloc(image->height * sizeof(unsigned char*));
    
    for (int y = 0; y < image->height; y++) {
        grayscale[y] = (unsigned char*)malloc(image->width * sizeof(unsigned char));
        dithered[y] = (unsigned char*)malloc(image->width * sizeof(unsigned char));
    }

    // Convert to grayscale
    for (int y = 0; y < image->height; y++) {
        png_bytep row = image->row_pointers[y];
        // Assuming 4 bytes per pixel (RGBA) after png_set_filler/png_set_gray_to_rgb
        for (int x = 0; x < image->width; x++) {
            png_bytep px = &(row[x * 4]);
            grayscale[y][x] = rgb_to_grayscale(px[0], px[1], px[2]);
        }
    }

    // Choose single-threaded for small images or multi-threaded for larger ones
    if (num_threads <= 1 || image->height * image->width < 10000) {
        printf("Running single-threaded dithering.\n");
        dither_image_st(grayscale, dithered, image->width, image->height);
    } else {
        printf("Running multi-threaded (wavefront) dithering with %d threads.\n", num_threads);
        dither_image_mt(grayscale, dithered, image->width, image->height, num_threads);
    }
    
    write_png_file(image_output, dithered, image->width, image->height);
    printf("File %s finished.\n", image_output);

    // Cleanup
    for (int y = 0; y < image->height; y++) {
        free(grayscale[y]);
        free(dithered[y]);
    }
    free(grayscale);
    free(dithered);
    free_png_image(image);

    return 0;
}
