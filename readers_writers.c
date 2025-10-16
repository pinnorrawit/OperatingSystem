/* 
 * Precision Thread Synchronization with Spinlocks
 * This program demonstrates precise timing control using high-resolution clocks
 * and spinlocks to coordinate thread execution in ordered ranges.
 */

#include <stdio.h>       // Standard I/O functions
#include <stdlib.h>      // Memory allocation, random numbers
#include <pthread.h>     // POSIX threads
#include <unistd.h>      // UNIX standard functions
#include <time.h>        // Time functions
#include <sys/time.h>    // System time functions
#include <semaphore.h>   // Semaphore synchronization
#include <math.h>        // Math functions (fabs, etc.)
#include <x86intrin.h>   // Intel intrinsics for _mm_pause()

// Program configuration constants
#define TOTAL_THREADS 1000      // Total number of threads to create
#define WRITER_THREADS 10       // Number of writer threads
#define READER_THREADS 990      // Number of reader threads

// Structure to hold individual thread information
typedef struct {
    int thread_id;      // Unique ID for each thread (0 to TOTAL_THREADS-1)
    int is_writer;      // Flag: 1 if writer thread, 0 if reader thread
    int range_group;    // Which range group this thread belongs to
} thread_data_t;

// Structure to track range execution information
typedef struct {
    int range_id;           // Unique identifier for this range
    int start_thread;       // First thread ID in this range
    int end_thread;         // Last thread ID in this range
    int is_writer_range;    // Flag: 1 if this range contains a writer
    int writer_id;          // Writer thread ID (if applicable, else -1)
    double start_time;      // When this range started execution (relative to program start)
    double duration;        // How long the critical section took
    double end_time;        // When this range finished (relative to program start)
} range_info_t;

// Global variables shared across all threads
sem_t range_semaphore;          // Semaphore to control range progression
pthread_mutex_t range_mutex;    // Mutex to protect shared state
int current_range = 0;          // Tracks which range is currently executing
int total_ranges = 0;           // Total number of ranges created
range_info_t* ranges;           // Dynamic array of range information
double program_start_time;      // Absolute start time of the program

// Function prototypes - declarations before implementations
void* thread_function(void* arg);                           // Main thread worker function
void initialize_ranges(int writer_indices[]);               // Setup range structures
void print_range_summary();                                 // Print final results
int determine_range_group(int thread_id, int writer_indices[]);  // Find which range a thread belongs to
double get_current_time_high_res();                         // High-precision timing function
void precise_spinlock_wait(double seconds);                 // Precise waiting function

/*
 * Main function - Program entry point
 * Coordinates thread creation, synchronization, and cleanup
 */
int main() {
    pthread_t threads[TOTAL_THREADS];           // Array to store thread identifiers
    thread_data_t thread_data[TOTAL_THREADS];   // Array to store thread-specific data
    int writer_indices[WRITER_THREADS];         // Array to store positions of writer threads
    int rc, i;                                  // Return code and loop counter
    
    // Record the absolute start time of the program using high-resolution timer
    program_start_time = get_current_time_high_res();
    
    // Print program header and configuration
    printf("=== Thread Creation with Ordered Critical Sections ===\n");
    printf("Total Threads: %d (Writers: %d, Readers: %d)\n\n", 
           TOTAL_THREADS, WRITER_THREADS, READER_THREADS);
    
    // Initialize synchronization primitives
    sem_init(&range_semaphore, 0, 0);           // Initialize semaphore with value 0 (blocking)
    pthread_mutex_init(&range_mutex, NULL);     // Initialize mutex with default attributes
    
    // Generate unique random positions for writer threads
    struct timespec seed_time;                  // High-resolution time structure for seeding
    clock_gettime(CLOCK_MONOTONIC, &seed_time); // Get current monotonic time
    srand(seed_time.tv_nsec);                   // Seed random generator with nanoseconds for better randomness
    
    // For each writer thread position we need to assign
    for (i = 0; i < WRITER_THREADS; i++) {
        int pos;        // Proposed position for writer
        int unique;     // Flag to check if position is unique
        
        // Keep generating positions until we find a unique one
        do {
            unique = 1;                         // Assume position is unique initially
            pos = rand() % TOTAL_THREADS;       // Generate random position between 0 and TOTAL_THREADS-1
            
            // Check if this position conflicts with any previously assigned writers
            for (int j = 0; j < i; j++) {
                if (writer_indices[j] == pos) {
                    unique = 0;                 // Mark as not unique if conflict found
                    break;                      // Exit inner loop early
                }
            }
        } while (!unique);                      // Repeat until unique position found
        
        writer_indices[i] = pos;                // Store the unique position
        printf("Writer at position: %d\n", pos); // Print writer position
    }
    printf("\n");                               // Blank line for output formatting
    
    // Initialize the range structures based on writer positions
    initialize_ranges(writer_indices);
    
    // Create all threads with their respective data
    printf("Creating %d threads...\n", TOTAL_THREADS);
    for (i = 0; i < TOTAL_THREADS; i++) {
        // Initialize thread data structure
        thread_data[i].thread_id = i;           // Set thread ID
        thread_data[i].is_writer = 0;           // Default to reader (will update if writer)
        thread_data[i].range_group = determine_range_group(i, writer_indices);  // Determine which range this thread belongs to
        
        // Check if this thread is a writer
        for (int j = 0; j < WRITER_THREADS; j++) {
            if (i == writer_indices[j]) {
                thread_data[i].is_writer = 1;   // Mark as writer thread
                break;                          // Exit inner loop once found
            }
        }
        
        // Create the thread with the thread function and pass thread data
        rc = pthread_create(&threads[i], NULL, thread_function, (void*)&thread_data[i]);
        if (rc) {
            // If thread creation failed, print error and exit
            printf("ERROR: pthread_create() failed for thread %d: %d\n", i, rc);
            exit(-1);
        }
    }
    printf("All threads created successfully.\n\n");
    
    // Start the execution sequence by unblocking the first waiting thread
    printf("Starting execution sequence...\n\n");
    sem_post(&range_semaphore);                 // Increment semaphore to allow first range to proceed
    
    // Wait for all threads to complete their execution
    for (i = 0; i < TOTAL_THREADS; i++) {
        pthread_join(threads[i], NULL);         // Block until thread i completes
    }
    
    // Print summary of execution results
    print_range_summary();
    
    // Cleanup resources
    sem_destroy(&range_semaphore);              // Destroy semaphore
    pthread_mutex_destroy(&range_mutex);        // Destroy mutex
    free(ranges);                               // Free dynamically allocated ranges array
    
    printf("Program completed successfully.\n");
    return 0;                                   // Exit program successfully
}

/*
 * High-resolution timing function
 * Returns current time in seconds with nanosecond precision
 * Uses CLOCK_MONOTONIC which is not affected by system time changes
 */
double get_current_time_high_res() {
    struct timespec ts;                         // timespec structure for seconds and nanoseconds
    clock_gettime(CLOCK_MONOTONIC, &ts);        // Get monotonic clock time (not affected by NTP adjustments)
    return ts.tv_sec + ts.tv_nsec * 1e-9;       // Convert to double: seconds + nanoseconds as fractional seconds
}

/*
 * Precise spinlock wait function
 * Implements a high-precision delay using busy waiting
 * Uses two-phase approach for balance between precision and CPU efficiency
 */
void precise_spinlock_wait(double seconds) {
    double start_time = get_current_time_high_res();  // Record start time with high precision
    double target_time = start_time + seconds;        // Calculate target end time
    
    // Phase 1: Aggressive spin for maximum precision (first 99.9% of wait)
    // This phase uses pure busy-waiting for the highest timing accuracy
    while (get_current_time_high_res() < target_time * 0.999) {
        asm volatile("" : : : "memory");        // Compiler memory barrier prevents optimization
    }
    
    // Phase 2: CPU-friendly final wait (last 0.1% of wait)
    // Uses pause instruction to reduce power consumption while maintaining precision
    while (get_current_time_high_res() < target_time) {
        _mm_pause();                            // Intel CPU pause instruction reduces power usage
        asm volatile("" : : : "memory");        // Compiler memory barrier
    }
}

/*
 * Initialize range structures based on writer thread positions
 * Creates alternating ranges of readers and writers
 */
void initialize_ranges(int writer_indices[]) {
    int i, j;                                   // Loop counters
    int start_range = 0;                        // Starting thread ID for current range
    int range_count = 0;                        // Counter for number of ranges created
    
    // Sort writer indices in ascending order using bubble sort
    // This ensures we process writers in increasing thread ID order
    for (i = 0; i < WRITER_THREADS - 1; i++) {
        for (j = 0; j < WRITER_THREADS - i - 1; j++) {
            if (writer_indices[j] > writer_indices[j + 1]) {
                // Swap positions if they're out of order
                int temp = writer_indices[j];
                writer_indices[j] = writer_indices[j + 1];
                writer_indices[j + 1] = temp;
            }
        }
    }
    
    // First pass: Count total number of ranges needed
    total_ranges = 0;                           // Initialize range counter
    start_range = 0;                            // Start from thread 0
    
    // Iterate through all writer positions to count ranges
    for (i = 0; i < WRITER_THREADS; i++) {
        // If there are readers before this writer, that's a reader range
        if (writer_indices[i] > start_range) total_ranges++;
        total_ranges++;                         // Count the writer range itself
        start_range = writer_indices[i] + 1;    // Move start position past this writer
    }
    
    // If there are readers after the last writer, count final reader range
    if (start_range < TOTAL_THREADS) total_ranges++;
    
    printf("Total ranges: %d\n", total_ranges); // Print total ranges count
    
    // Allocate memory for ranges array
    ranges = malloc(total_ranges * sizeof(range_info_t));
    if (ranges == NULL) {
        printf("ERROR: Failed to allocate memory for ranges\n");
        exit(-1);
    }
    
    // Second pass: Initialize each range structure
    start_range = 0;                            // Reset start position
    range_count = 0;                            // Reset range counter
    
    // Create ranges for each writer and the readers around them
    for (i = 0; i < WRITER_THREADS; i++) {
        int writer_pos = writer_indices[i];     // Current writer position
        
        // Create reader range before writer (if any readers exist before writer)
        if (writer_pos > start_range) {
            ranges[range_count].range_id = range_count;             // Set range ID
            ranges[range_count].start_thread = start_range;         // First thread in range
            ranges[range_count].end_thread = writer_pos - 1;        // Last thread in range
            ranges[range_count].is_writer_range = 0;                // Mark as reader range
            ranges[range_count].writer_id = -1;                     // No writer in this range
            range_count++;                                          // Move to next range
        }
        
        // Create writer range (single thread)
        ranges[range_count].range_id = range_count;                 // Set range ID
        ranges[range_count].start_thread = writer_pos;              // Writer thread is both start and end
        ranges[range_count].end_thread = writer_pos;                // Single thread range
        ranges[range_count].is_writer_range = 1;                    // Mark as writer range
        ranges[range_count].writer_id = writer_pos;                 // Store writer thread ID
        range_count++;                                              // Move to next range
        
        start_range = writer_pos + 1;           // Update start position past this writer
    }
    
    // Create final reader range if there are readers after the last writer
    if (start_range < TOTAL_THREADS) {
        ranges[range_count].range_id = range_count;                 // Set range ID
        ranges[range_count].start_thread = start_range;             // First thread in final range
        ranges[range_count].end_thread = TOTAL_THREADS - 1;         // Last thread in final range
        ranges[range_count].is_writer_range = 0;                    // Mark as reader range
        ranges[range_count].writer_id = -1;                         // No writer in this range
    }
    
    // Print range configuration for verification
    printf("Range Configuration:\n");
    for (i = 0; i < total_ranges; i++) {
        if (ranges[i].is_writer_range) {
            // Print writer range information
            printf("Range %d: Writer%d\n", i + 1, ranges[i].writer_id);
        } else {
            if (ranges[i].start_thread == ranges[i].end_thread) {
                // Print single-reader range
                printf("Range %d: Reader%d\n", i + 1, ranges[i].start_thread);
            } else {
                // Print multi-reader range
                printf("Range %d: Readers %d-%d\n", i + 1, ranges[i].start_thread, ranges[i].end_thread);
            }
        }
    }
    printf("\n");                               // Blank line for output formatting
}

/*
 * Determine which range group a thread belongs to
 * Returns the range index for the given thread ID
 */
int determine_range_group(int thread_id, int writer_indices[]) {
    int group = 0;                              // Start with group 0
    int start_range = 0;                        // Start checking from thread 0
    
    // Iterate through all writer positions to find where this thread belongs
    for (int i = 0; i < WRITER_THREADS; i++) {
        int writer_pos = writer_indices[i];     // Current writer position
        
        // Check if thread is in reader range before current writer
        if (thread_id >= start_range && thread_id < writer_pos) {
            return group;                       // Return current group (reader range before writer)
        }
        group++;                                // Move to next group (this reader range)
        
        // Check if thread is the writer itself
        if (thread_id == writer_pos) {
            return group;                       // Return current group (writer range)
        }
        group++;                                // Move to next group (past this writer)
        
        start_range = writer_pos + 1;           // Update start position past this writer
    }
    
    // If we get here, thread is in the final reader range (after all writers)
    if (thread_id >= start_range && thread_id < TOTAL_THREADS) {
        return group;                           // Return final group
    }
    
    return -1;  // Should never happen if thread_id is valid
}

/*
 * Main thread worker function
 * Executed by each thread to wait for its turn and perform critical section
 */
void* thread_function(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;  // Cast argument to thread data structure
    int my_range = data->range_group;           // Get which range this thread belongs to
    
    // Wait for this thread's turn to execute
    while (1) {
        pthread_mutex_lock(&range_mutex);       // Acquire mutex to check current range safely
        
        // Check if it's this thread's range turn to execute
        if (current_range == my_range) {
            // If this is the first thread in the range, record start time
            if (data->thread_id == ranges[my_range].start_thread) {
                ranges[my_range].start_time = get_current_time_high_res() - program_start_time;
                printf("Range %d START: ", my_range + 1);
                if (ranges[my_range].is_writer_range) {
                    printf("Writer%d\n", ranges[my_range].writer_id);
                } else {
                    printf("Readers %d-%d\n", ranges[my_range].start_thread, ranges[my_range].end_thread);
                }
            }
            pthread_mutex_unlock(&range_mutex); // Release mutex before critical section
            break;                              // Exit wait loop and proceed to critical section
        }
        pthread_mutex_unlock(&range_mutex);     // Release mutex if not this range's turn
        
        // Short pause to reduce CPU contention while waiting
        _mm_pause();                            // CPU pause instruction
    }
    
    // Critical section - execute for exactly 1 second using precise timing
    double start_cs = get_current_time_high_res();      // Record critical section start time
    precise_spinlock_wait(1.0);                         // Wait exactly 1 second with high precision
    double end_cs = get_current_time_high_res();        // Record critical section end time
    double duration = end_cs - start_cs;                // Calculate actual duration
    
    // Last thread in the range advances to next range
    pthread_mutex_lock(&range_mutex);           // Acquire mutex to update shared state
    
    // Double-check that we're still in the correct range (safety check)
    if (current_range == my_range) {
        // Check if this thread is the last one in its range
        if ((data->is_writer && data->thread_id == ranges[my_range].writer_id) ||
            (!data->is_writer && data->thread_id == ranges[my_range].end_thread)) {
            
            ranges[my_range].duration = duration;       // Store actual duration
            ranges[my_range].end_time = get_current_time_high_res() - program_start_time;  // Store end time
            
            // Print range completion information
            printf("Range %d END  : ", my_range + 1);
            if (ranges[my_range].is_writer_range) {
                printf("Writer%d | Duration: %.6fs | Error: %+.6fs\n", 
                       ranges[my_range].writer_id, duration, duration - 1.0);
            } else {
                printf("Readers %d-%d | Duration: %.6fs | Error: %+.6fs\n", 
                       ranges[my_range].start_thread, ranges[my_range].end_thread, 
                       duration, duration - 1.0);
            }
            
            current_range++;                    // Advance to next range
            sem_post(&range_semaphore);         // Signal waiting threads to check new range
        }
    }
    pthread_mutex_unlock(&range_mutex);         // Release mutex
    
    pthread_exit(NULL);                         // Exit thread successfully
}

/*
 * Print comprehensive summary of execution results
 * Shows timing statistics, accuracy measurements, and detailed range information
 */
void print_range_summary() {
    printf("\n=== EXECUTION SUMMARY ===\n");
    printf("=========================\n");
    
    // Calculate overall program timing
    double total_program_time = get_current_time_high_res() - program_start_time;
    double total_critical_time = 0.0;           // Will accumulate all critical section times
    
    // Calculate statistics for critical section durations
    double min_duration = 1000.0;               // Initialize with large value
    double max_duration = 0.0;                  // Initialize with small value
    double avg_duration = 0.0;                  // Will calculate average
    
    // Process all ranges to calculate statistics
    for (int i = 0; i < total_ranges; i++) {
        total_critical_time += ranges[i].duration;          // Sum all critical section times
        if (ranges[i].duration < min_duration) min_duration = ranges[i].duration;  // Track minimum
        if (ranges[i].duration > max_duration) max_duration = ranges[i].duration;  // Track maximum
        avg_duration += ranges[i].duration;                 // Accumulate for average
    }
    avg_duration /= total_ranges;               // Calculate average duration
    
    // Print program statistics
    printf("Program Statistics:\n");
    printf("  Total Threads: %d\n", TOTAL_THREADS);
    printf("  Writer Threads: %d\n", WRITER_THREADS);
    printf("  Reader Threads: %d\n", READER_THREADS);
    printf("  Total Ranges: %d\n", total_ranges);
    printf("  Expected Duration: %.3f seconds\n", total_ranges * 1.0);  // Ideal case: 1 second per range
    printf("  Actual Duration: %.3f seconds\n", total_program_time);    // Measured total time
    printf("  Efficiency: %.3f%%\n", (total_ranges * 1.0 / total_program_time) * 100);  // How close to ideal
    
    // Print critical section timing accuracy
    printf("\nCritical Section Accuracy:\n");
    printf("  Average Duration: %.6f seconds\n", avg_duration);
    printf("  Min Duration: %.6f seconds\n", min_duration);
    printf("  Max Duration: %.6f seconds\n", max_duration);
    printf("  Average Deviation: %.6f seconds\n", fabs(avg_duration - 1.0));  // Absolute deviation from 1 second
    printf("  Standard Deviation Calculation would show timing consistency\n");
    
    // Print detailed range-by-range timing information
    printf("\nRange Timing Details:\n");
    printf("Range | Type    | Thread(s)       | Start Time | Duration  | Error\n");
    printf("------|---------|-----------------|------------|-----------|---------\n");
    
    for (int i = 0; i < total_ranges; i++) {
        const char* type = ranges[i].is_writer_range ? "Writer" : "Readers";  // Range type string
        
        if (ranges[i].is_writer_range) {
            // Format output for writer range
            printf("%5d | %-7s | %-15d | %9.4f | %9.6f | %+9.6f\n",
                   i + 1, type, ranges[i].writer_id, ranges[i].start_time, 
                   ranges[i].duration, ranges[i].duration - 1.0);
        } else {
            if (ranges[i].start_thread == ranges[i].end_thread) {
                // Format output for single-reader range
                printf("%5d | %-7s | %-15d | %9.4f | %9.6f | %+9.6f\n",
                       i + 1, type, ranges[i].start_thread, ranges[i].start_time, 
                       ranges[i].duration, ranges[i].duration - 1.0);
            } else {
                // Format output for multi-reader range
                printf("%5d | %-7s | %4d-%-10d | %9.4f | %9.6f | %+9.6f\n",
                       i + 1, type, ranges[i].start_thread, ranges[i].end_thread, 
                       ranges[i].start_time, ranges[i].duration, ranges[i].duration - 1.0);
            }
        }
    }
}
