#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h> // Necessary for omp_get_wtime()

// --- Configuration ---
#define MAX_THREADS 6
#define EXECUTABLE_NAME "./thread" // Changed to match your executable name
#define INPUT_FILE "input.png"     // *** CHANGE THIS to your input PNG file ***
#define OUTPUT_FILE "output.png"   // Temporary output file name
#define RESULT_FILE "dithering_performance.csv"
#define RUNS_PER_THREAD 5          // Number of times to run each thread count for averaging

/**
 * @brief Executes the dithering program and measures the time.
 * @param threads The number of threads to pass to the dither program.
 * @return The average execution time in seconds.
 */
double run_dither_and_time(int threads) {
    char command[512];
    double total_time = 0.0;
    
    // The command string to execute the dithering program
    // Format: ./thread input.png output.png <threads>
    snprintf(command, sizeof(command), "%s %s %s %d", 
             EXECUTABLE_NAME, INPUT_FILE, OUTPUT_FILE, threads);
    
    printf("  Running with %d threads (x%d times)...\n", threads, RUNS_PER_THREAD);

    for (int i = 0; i < RUNS_PER_THREAD; i++) {
        double start_time = omp_get_wtime();
        
        // Use system() to execute the compiled thread program
        int result = system(command);
        
        double end_time = omp_get_wtime();
        
        if (result != 0) {
            fprintf(stderr, "Error: Program %s failed for %d threads. Exiting.\n", EXECUTABLE_NAME, threads);
            return -1.0; // Indicate failure
        }
        total_time += (end_time - start_time);
    }

    return total_time / RUNS_PER_THREAD;
}

int main() {
    FILE *fp;
    
    printf("--- Performance Analysis Tool ---\n");
    printf("Target executable: %s\n", EXECUTABLE_NAME);
    printf("Input file: %s\n", INPUT_FILE);
    printf("Saving results to: %s\n", RESULT_FILE);
    printf("---------------------------------\n");

    // 1. Open the CSV file for writing
    fp = fopen(RESULT_FILE, "w");
    if (fp == NULL) {
        perror("Could not open results file");
        return 1;
    }

    // Write CSV header
    fprintf(fp, "Threads,Average_Time_sec,Speedup\n");

    double baseline_time = 0.0;

    // 2. Loop from 1 to MAX_THREADS
    for (int threads = 1; threads <= MAX_THREADS; threads++) {
        double avg_time = run_dither_and_time(threads);

        if (avg_time < 0) {
            // Error occurred during run_dither_and_time
            fclose(fp);
            return 1;
        }

        // Set the baseline time (sequential run)
        if (threads == 1) {
            baseline_time = avg_time;
            printf("  Baseline (1 thread) time: %.4f seconds\n", baseline_time);
        }

        // Calculate Speedup (Time_sequential / Time_parallel)
        double speedup = baseline_time / avg_time;

        printf("  Result: Time = %.4f s, Speedup = %.2fx\n\n", avg_time, speedup);

        // Write data to CSV file
        fprintf(fp, "%d,%.6f,%.6f\n", threads, avg_time, speedup);
    }

    // 3. Close file and finish
    fclose(fp);
    printf("Analysis complete. Data saved to %s.\n", RESULT_FILE);

    return 0;
}

