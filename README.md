# 💻 Operating Systems Projects Guide

This repository contains a collection of C and Python programs demonstrating core Operating System concepts, including **Process Management (Fork)**, **Network Sockets (Server/Client)**, **Thread Synchronization (Readers/Writers)**, and **Multithreading Performance Analysis**.

All C programs are designed to be compiled and run on **Linux**.

---

## 1. Process Management: FORK

Demonstrates creating multiple concurrent child processes using the `fork()` system call.

| File | Description |
| :--- | :--- |
| `fork.c` | Creates 10 child processes, each pauses for 3 seconds, while the parent waits for all to finish. |

### Compilation and Run

| Action | Command |
| :--- | :--- |
| **Compile** | `gcc -o fork fork.c` |
| **Run** | `./fork` |

---

## 2. Network Sockets: Server & Client (TCP)

A multi-threaded TCP server that sends the current system time to a client every second.

| File | Description |
| :--- | :--- |
| `server.c` | Multi-threaded TCP server listening on port 6013. Spawns a new thread for each client. |
| `client.c` | Connects to `127.0.0.1:6013` and continuously prints the time received from the server. |

### Compilation and Run

| Action | File | Command | Notes |
| :--- | :--- | :--- | :--- |
| **Compile** | `server.c` | `gcc -o server server.c -pthread` | Requires the **POSIX threads** library. |
| **Compile** | `client.c` | `gcc -o client client.c` | Standard compilation. |
| **Run** | **Server** | `./server` | **Must be run first** in a separate terminal. |
| **Run** | **Client** | `./client` | Can open multiple client windows concurrently. |
| **Stop** | N/A | `Ctrl+C` | Use in all active terminal windows. |

---

## 3. Thread Synchronization: Readers/Writers

Programs that implement a strict, ordered execution of the Readers-Writers problem, timing critical sections for 1.0 seconds.

| File | Description |
| :--- | :--- |
| `readers_writers.c` | Uses **spinlocks** (`_mm_pause`) for high-precision timing of the critical section. |
| `testrw.c` | Uses **`nanosleep()`** for timing the critical section. |

### Compilation and Run

| Action | File | Command | Notes |
| :--- | :--- | :--- | :--- |
| **Compile (Spinlock)** | `readers_writers.c` | `gcc -o rw_spinlock readers_writers.c -pthread -lrt -lm -O2` | **Recommended:** `-O2` for best timing accuracy. |
| **Compile (Nanosleep)** | `testrw.c` | `gcc -o rw_nanosleep testrw.c -pthread -lrt -lm` | `-lrt` is often needed for high-resolution timing functions. |
| **Run** | **Spinlock** | `./rw_spinlock` | |
| **Run** | **Nanosleep**| `./rw_nanosleep` | |

---

## 4. Multithreading Performance Analysis (Dithering)

This suite implements the Floyd-Steinberg error diffusion dithering algorithm in C (single and multi-threaded) and provides Python tools for analysis and plotting.

### A. Dithering Implementations (C)

| File | Description |
| :--- | :--- |
| `error_diffusion.c` | **Single-Threaded** C implementation of dithering. |
| `thread.c` | **Multi-Threaded** C implementation using a **wavefront** synchronization pattern. |

#### Compilation and Run

| Action | File | Command |
| :--- | :--- | :--- |
| **Compile (ST)** | `error_diffusion.c` | `gcc -o error_diffusion error_diffusion.c -lm -lpng` |
| **Compile (MT)** | `thread.c` | `gcc -o thread thread.c -lm -lpng -lpthread` |
| **Run (ST)** | N/A | `./error_diffusion <input_file.png> <output_file.png>` |
| **Run (MT)** | N/A | `./thread <input_file.png> <output_file.png> <num_threads>` |

### B. Analysis and Plotting (C & Python)

These files are used to benchmark the performance of the multi-threaded dithering program.

#### Prerequisites

1.  Ensure you have a compiled multi-threaded executable named `./thread`.
2.  Ensure you have a PNG image named **`input.png`** in the same directory.
3.  Install Python libraries: `pip install pandas matplotlib numpy`

| File | Description |
| :--- | :--- |
| `analysis.c` | Runs the `./thread` executable repeatedly for thread counts $1$ to $N$, measures the average time, calculates speedup, and saves results to `dithering_performance.csv`. |
| `plot.py` | Reads `dithering_performance.csv` and generates a visualization of Execution Time and Speedup vs. Thread Count. |

#### Compilation and Run

| Step | File | Command | Notes |
| :--- | :--- | :--- | :--- |
| **1. Compile** | `analysis.c` | `gcc -o analysis analysis.c -lpng -lm -pthread -fopenmp` | **Requires** the **OpenMP** flag (`-fopenmp`). |
| **2. Run Analysis** | `analysis.c` | `./analysis` | This generates the **`dithering_performance.csv`** file. |
| **3. Run Plot** | `plot.py` | `python3 plot.py` | Displays the final performance graph. |

### C. Reference and Comparison by "ส้มซ่า" (Python)

Used to generate a reference dithered image and measure the similarity between two images.

| File | Description |
| :--- | :--- |
| `error_diffusion.py`| **Python-based** dithering (used to create a reference image). |
| `bw_similarity.py` | Compares the pixel-by-pixel similarity between two 1-bit images. |

#### Run Commands

| Action | Command |
| :--- | :--- |
| **Generate Reference** | `python3 error_diffusion.py <input.jpg> <ref_output.png>` |
| **Compare Images** | `python3 bw_similarity.py <image1.png> <image2.png>` |
