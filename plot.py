import pandas as pd
import matplotlib.pyplot as plt

# --- Configuration ---
CSV_FILE = 'dithering_performance.csv'
PLOT_TITLE = "Multithreaded Dithering: Execution Time Analysis"
BAR_WIDTH = 0.7

def plot_performance(csv_file):
    """Reads performance data from a CSV and generates a single Time vs. CPU bar plot with Speedup details."""
    try:
        # Read the CSV file into a pandas DataFrame
        df = pd.read_csv(csv_file)
    except FileNotFoundError:
        print(f"Error: The file '{csv_file}' was not found.")
        print("Please ensure your C 'analysis' program has run successfully and generated the CSV.")
        return
    except Exception as e:
        print(f"An error occurred while reading the CSV: {e}")
        return

    # Set up the figure for a single subplot
    fig, ax = plt.subplots(1, 1, figsize=(10, 6))
    fig.suptitle(PLOT_TITLE, fontsize=16)

    # ----------------------------------------
    # Plot: Time vs. Threads (Bar Chart)
    # ----------------------------------------
    
    # Measured time as bar graph
    ax.bar(df['Threads'], df['Average_Time_sec'], width=BAR_WIDTH, color='#007BFF', alpha=0.9, label='Execution Time (s)', zorder=3)

    ax.set_title('Execution Time by Thread Count (Lower is Better)')
    ax.set_xlabel('Number of Threads (CPU Cores)')
    ax.set_ylabel('Average Execution Time (seconds)')
    ax.grid(axis='y', linestyle='-', alpha=0.7)
    ax.set_xticks(df['Threads']) 
    
    # Set Y-axis limit to clearly frame the time values
    ax.set_ylim(bottom=0, top=df['Average_Time_sec'].max() * 1.25) # Extra space for annotations
    
    # Add annotations (Time on top, Speedup detail below)
    if not df.empty:
        max_time = df['Average_Time_sec'].max()
        
        for i in range(len(df)):
            threads = df['Threads'][i]
            time = df['Average_Time_sec'][i]
            speedup = df['Speedup'][i]
            
            # Time annotation (always on top)
            ax.text(threads, time + (max_time * 0.03), f'{time:.3f}s', 
                     ha='center', va='bottom', fontsize=10, color='black', fontweight='bold', zorder=4)

            # Speedup annotation (only for threads > 1)
            if threads > 1:
                 ax.text(threads, time + (max_time * 0.1), f'(+{speedup:.2f}x)', 
                     ha='center', va='bottom', fontsize=9, color='red', zorder=4)


    # Show the plots
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_performance(CSV_FILE)

