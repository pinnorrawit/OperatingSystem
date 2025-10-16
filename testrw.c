#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <semaphore.h>
#include <math.h>

#define TOTAL_THREADS 1000
#define WRITER_THREADS 10
#define READER_THREADS 990

// Structure to hold thread information
typedef struct {
    int thread_id;
    int is_writer;
    int range_group;
} thread_data_t;

// Structure for range information
typedef struct {
    int range_id;
    int start_thread;
    int end_thread;
    int is_writer_range;
    int writer_id;
    double start_time;
    double duration;
} range_info_t;

// Global variables
sem_t range_semaphore;
pthread_mutex_t range_mutex;
int current_range = 0;
int total_ranges = 0;
range_info_t* ranges;
double program_start_time;

// Function prototypes
void* thread_function(void* arg);
void initialize_ranges(int writer_indices[]);
void print_range_summary();
int determine_range_group(int thread_id, int writer_indices[]);
double get_current_time();
void precise_sleep(double seconds);

int main() {
    pthread_t threads[TOTAL_THREADS];
    thread_data_t thread_data[TOTAL_THREADS];
    int writer_indices[WRITER_THREADS];
    int rc, i;
    
    program_start_time = get_current_time();
    
    printf("=== Thread Creation with Ordered Critical Sections ===\n");
    printf("Total Threads: %d (Writers: %d, Readers: %d)\n\n", 
           TOTAL_THREADS, WRITER_THREADS, READER_THREADS);
    
    // Initialize synchronization primitives
    sem_init(&range_semaphore, 0, 0);
    pthread_mutex_init(&range_mutex, NULL);
    
    // Select random positions for writer threads
    srand(time(NULL));
    for (i = 0; i < WRITER_THREADS; i++) {
        int pos;
        int unique;
        
        do {
            unique = 1;
            pos = rand() % TOTAL_THREADS;
            
            for (int j = 0; j < i; j++) {
                if (writer_indices[j] == pos) {
                    unique = 0;
                    break;
                }
            }
        } while (!unique);
        
        writer_indices[i] = pos;
        printf("Writer at position: %d\n", pos);
    }
    printf("\n");
    
    // Initialize ranges
    initialize_ranges(writer_indices);
    
    // Create all threads
    printf("Creating %d threads...\n", TOTAL_THREADS);
    for (i = 0; i < TOTAL_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].is_writer = 0;
        thread_data[i].range_group = determine_range_group(i, writer_indices);
        
        for (int j = 0; j < WRITER_THREADS; j++) {
            if (i == writer_indices[j]) {
                thread_data[i].is_writer = 1;
                break;
            }
        }
        
        rc = pthread_create(&threads[i], NULL, thread_function, (void*)&thread_data[i]);
        if (rc) {
            printf("ERROR: pthread_create() failed for thread %d: %d\n", i, rc);
            exit(-1);
        }
    }
    printf("All threads created successfully.\n\n");
    
    // Start execution
    printf("Starting execution sequence...\n\n");
    sem_post(&range_semaphore);
    
    // Wait for all threads to complete
    for (i = 0; i < TOTAL_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    print_range_summary();
    
    // Cleanup
    sem_destroy(&range_semaphore);
    pthread_mutex_destroy(&range_mutex);
    free(ranges);
    
    printf("Program completed successfully.\n");
    return 0;
}

double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void precise_sleep(double seconds) {
    struct timespec req, rem;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - req.tv_sec) * 1e9);
    nanosleep(&req, &rem);
}

void initialize_ranges(int writer_indices[]) {
    int i, j;
    int start_range = 0;
    int range_count = 0;
    
    // Sort writer indices
    for (i = 0; i < WRITER_THREADS - 1; i++) {
        for (j = 0; j < WRITER_THREADS - i - 1; j++) {
            if (writer_indices[j] > writer_indices[j + 1]) {
                int temp = writer_indices[j];
                writer_indices[j] = writer_indices[j + 1];
                writer_indices[j + 1] = temp;
            }
        }
    }
    
    // Count total ranges
    total_ranges = 0;
    start_range = 0;
    for (i = 0; i < WRITER_THREADS; i++) {
        if (writer_indices[i] > start_range) total_ranges++;
        total_ranges++;
        start_range = writer_indices[i] + 1;
    }
    if (start_range < TOTAL_THREADS) total_ranges++;
    
    printf("Total ranges: %d\n", total_ranges);
    
    // Allocate and initialize ranges
    ranges = malloc(total_ranges * sizeof(range_info_t));
    if (ranges == NULL) {
        printf("ERROR: Failed to allocate memory for ranges\n");
        exit(-1);
    }
    
    start_range = 0;
    range_count = 0;
    for (i = 0; i < WRITER_THREADS; i++) {
        int writer_pos = writer_indices[i];
        
        // Reader range before writer
        if (writer_pos > start_range) {
            ranges[range_count].range_id = range_count;
            ranges[range_count].start_thread = start_range;
            ranges[range_count].end_thread = writer_pos - 1;
            ranges[range_count].is_writer_range = 0;
            ranges[range_count].writer_id = -1;
            range_count++;
        }
        
        // Writer range
        ranges[range_count].range_id = range_count;
        ranges[range_count].start_thread = writer_pos;
        ranges[range_count].end_thread = writer_pos;
        ranges[range_count].is_writer_range = 1;
        ranges[range_count].writer_id = writer_pos;
        range_count++;
        
        start_range = writer_pos + 1;
    }
    
    // Final reader range
    if (start_range < TOTAL_THREADS) {
        ranges[range_count].range_id = range_count;
        ranges[range_count].start_thread = start_range;
        ranges[range_count].end_thread = TOTAL_THREADS - 1;
        ranges[range_count].is_writer_range = 0;
        ranges[range_count].writer_id = -1;
    }
    
    // Print range configuration
    printf("Range Configuration:\n");
    for (i = 0; i < total_ranges; i++) {
        if (ranges[i].is_writer_range) {
            printf("Range %d: Writer%d\n", i + 1, ranges[i].writer_id);
        } else {
            if (ranges[i].start_thread == ranges[i].end_thread) {
                printf("Range %d: Reader%d\n", i + 1, ranges[i].start_thread);
            } else {
                printf("Range %d: Readers %d-%d\n", i + 1, ranges[i].start_thread, ranges[i].end_thread);
            }
        }
    }
    printf("\n");
}

int determine_range_group(int thread_id, int writer_indices[]) {
    int group = 0;
    int start_range = 0;
    
    for (int i = 0; i < WRITER_THREADS; i++) {
        int writer_pos = writer_indices[i];
        
        if (thread_id >= start_range && thread_id < writer_pos) {
            return group;
        }
        group++;
        
        if (thread_id == writer_pos) {
            return group;
        }
        group++;
        
        start_range = writer_pos + 1;
    }
    
    if (thread_id >= start_range && thread_id < TOTAL_THREADS) {
        return group;
    }
    
    return -1;
}

void* thread_function(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int my_range = data->range_group;
    
    // Wait for our turn
    while (1) {
        pthread_mutex_lock(&range_mutex);
        if (current_range == my_range) {
            // First thread in this range records start time
            if (data->thread_id == ranges[my_range].start_thread) {
                ranges[my_range].start_time = get_current_time() - program_start_time;
                printf("Range %d START: ", my_range + 1);
                if (ranges[my_range].is_writer_range) {
                    printf("Writer%d\n", ranges[my_range].writer_id);
                } else {
                    printf("Readers %d-%d\n", ranges[my_range].start_thread, ranges[my_range].end_thread);
                }
            }
            pthread_mutex_unlock(&range_mutex);
            break;
        }
        pthread_mutex_unlock(&range_mutex);
        
        // Wait for signal
        sem_wait(&range_semaphore);
        sem_post(&range_semaphore);
        usleep(1000);
    }
    
    // Critical section - exactly 1 second
    double start_cs = get_current_time();
    precise_sleep(1.0);
    double end_cs = get_current_time();
    double duration = end_cs - start_cs;
    
    // Last thread in range moves to next range
    pthread_mutex_lock(&range_mutex);
    if (current_range == my_range) {
        if ((data->is_writer && data->thread_id == ranges[my_range].writer_id) ||
            (!data->is_writer && data->thread_id == ranges[my_range].end_thread)) {
            
            ranges[my_range].duration = duration;
            
            printf("Range %d END  : ", my_range + 1);
            if (ranges[my_range].is_writer_range) {
                printf("Writer%d | Duration: %.4fs\n", ranges[my_range].writer_id, duration);
            } else {
                printf("Readers %d-%d | Duration: %.4fs\n", ranges[my_range].start_thread, ranges[my_range].end_thread, duration);
            }
            
            current_range++;
            sem_post(&range_semaphore);
        }
    }
    pthread_mutex_unlock(&range_mutex);
    
    pthread_exit(NULL);
}

void print_range_summary() {
    printf("\n=== EXECUTION SUMMARY ===\n");
    printf("=========================\n");
    
    double total_program_time = get_current_time() - program_start_time;
    double total_critical_time = 0.0;
    
    // Calculate statistics
    double min_duration = 1000.0, max_duration = 0.0, avg_duration = 0.0;
    for (int i = 0; i < total_ranges; i++) {
        total_critical_time += ranges[i].duration;
        if (ranges[i].duration < min_duration) min_duration = ranges[i].duration;
        if (ranges[i].duration > max_duration) max_duration = ranges[i].duration;
        avg_duration += ranges[i].duration;
    }
    avg_duration /= total_ranges;
    
    printf("Program Statistics:\n");
    printf("  Total Threads: %d\n", TOTAL_THREADS);
    printf("  Writer Threads: %d\n", WRITER_THREADS);
    printf("  Reader Threads: %d\n", READER_THREADS);
    printf("  Total Ranges: %d\n", total_ranges);
    printf("  Expected Duration: %.3f seconds\n", total_ranges * 1.0);
    printf("  Actual Duration: %.3f seconds\n", total_program_time);
    printf("  Efficiency: %.3f%%\n", (total_ranges * 1.0 / total_program_time) * 100);
    
    printf("\nCritical Section Accuracy:\n");
    printf("  Average Duration: %.6f seconds\n", avg_duration);
    printf("  Min Duration: %.6f seconds\n", min_duration);
    printf("  Max Duration: %.6f seconds\n", max_duration);
    printf("  Average Deviation: %.6f seconds\n", fabs(avg_duration - 1.0));
    
    printf("\nRange Timing Details:\n");
    printf("Range | Type    | Thread(s)       | Start Time | Duration\n");
    printf("------|---------|-----------------|------------|---------\n");
    
    for (int i = 0; i < total_ranges; i++) {
        const char* type = ranges[i].is_writer_range ? "Writer" : "Readers";
        
        if (ranges[i].is_writer_range) {
            printf("%5d | %-7s | %-15d | %9.4f | %8.4f\n",
                   i + 1, type, ranges[i].writer_id, ranges[i].start_time, ranges[i].duration);
        } else {
            if (ranges[i].start_thread == ranges[i].end_thread) {
                printf("%5d | %-7s | %-15d | %9.4f | %8.4f\n",
                       i + 1, type, ranges[i].start_thread, ranges[i].start_time, ranges[i].duration);
            } else {
                printf("%5d | %-7s | %4d-%-10d | %9.4f | %8.4f\n",
                       i + 1, type, ranges[i].start_thread, ranges[i].end_thread, 
                       ranges[i].start_time, ranges[i].duration);
            }
        }
    }
}
