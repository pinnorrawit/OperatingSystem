#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <string.h>
#include <math.h>

typedef struct {
    int width;
    int height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep *row_pointers;
} PngImage;

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

void dither_image(unsigned char** input, unsigned char** output, int width, int height) {
    // Create working array
    int** work = (int**)malloc(height * sizeof(int*));
    for (int y = 0; y < height; y++) {
        work[y] = (int*)malloc(width * sizeof(int));
        for (int x = 0; x < width; x++) {
            work[y][x] = input[y][x];
        }
    }

    // Floyd-Steinberg dithering with Python-compatible floor division
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int old_pixel = work[y][x];
            int new_pixel = (old_pixel > 128) ? 255 : 0;
            output[y][x] = (unsigned char)new_pixel;
            int err = old_pixel - new_pixel;

            // Use floor division to match Python's //
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

    // Cleanup
    for (int y = 0; y < height; y++) {
        free(work[y]);
    }
    free(work);
}
int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 3) {
        printf("Usage: %s <input.png> <output.png>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* image_output = argv[2];

    // Read PNG
    PngImage *image = read_png_file(input_file);
    if (!image) {
        printf("Error: Could not read %s\n", input_file);
        return 1;
    }

    // Allocate arrays
    unsigned char** grayscale = (unsigned char**)malloc(image->height * sizeof(unsigned char*));
    unsigned char** dithered = (unsigned char**)malloc(image->height * sizeof(unsigned char*));
    
    if (!grayscale || !dithered) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }
    
    for (int y = 0; y < image->height; y++) {
        grayscale[y] = (unsigned char*)malloc(image->width * sizeof(unsigned char));
        dithered[y] = (unsigned char*)malloc(image->width * sizeof(unsigned char));
        if (!grayscale[y] || !dithered[y]) {
            printf("Error: Memory allocation failed for row %d\n", y);
            return 1;
        }
    }

    // Convert to grayscale
    for (int y = 0; y < image->height; y++) {
        png_bytep row = image->row_pointers[y];
        for (int x = 0; x < image->width; x++) {
            png_bytep px = &(row[x * 4]);
            grayscale[y][x] = rgb_to_grayscale(px[0], px[1], px[2]);
        }
    }

    // Create dithered image
    dither_image(grayscale, dithered, image->width, image->height);
    write_png_file(image_output, dithered, image->width, image->height);
    
    printf("File %s finished\n", image_output);

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
