#!/bin/bash

# File to store results in text
output_file="timing_results.txt"
echo "N_Threads Real_Time User_Time Sys_Time" > "$output_file"

# Function to extract and convert time
extract_and_convert_time() {
    time_str=$1
    # Convert time format from minutes:seconds to total seconds
    min=$(echo $time_str | cut -d'm' -f 1)
    sec=$(echo $time_str | cut -d'm' -f 2 | cut -d's' -f 1)
    echo "scale=3; $min*60 + $sec" | bc
}

# Function to run the program and extract times
run_and_record_time() {
    N=$1
    # Run your program with time command and capture the output
    time_output=$( (time ./bench $N) 2>&1 )

    # Extract and convert the real, user, and sys time
    real_time=$(extract_and_convert_time $(echo "$time_output" | grep real | awk '{print $2}'))
    user_time=$(extract_and_convert_time $(echo "$time_output" | grep user | awk '{print $2}'))
    sys_time=$(extract_and_convert_time $(echo "$time_output" | grep sys | awk '{print $2}'))

    # Write to the file
    echo "$N $real_time $user_time $sys_time" >> "$output_file"
}

# Test with predefined thread counts
for N in 1 2 3 5 6 8 10 15 20 30 40 50 60 70 80 90 100
do
    run_and_record_time $N
    echo "$N threads task done"
done

# Python script for plotting and saving the graph
python - <<END
import matplotlib.pyplot as plt
import pandas as pd

def convert_time(time_val):
    if isinstance(time_val, str):
        minutes, seconds = time_val.strip().split('m')
        seconds = seconds[:-1]  # Remove trailing 's'
        return float(minutes) * 60 + float(seconds)
    elif isinstance(time_val, float):
        return time_val  # Already in seconds
    else:
        raise ValueError("Unsupported time value format")

def plot_and_save(data, y_values, y_label, file_name):
    plt.figure(figsize=(10, 6))
    plt.plot(data['N_Threads'], data[y_values], label=y_label, marker='o')
    plt.title(f'Performance Analysis: {y_label}')
    plt.xlabel('Number of Threads')
    plt.ylabel('Time (seconds)')
    plt.grid(True)
    plt.savefig(file_name)
    plt.close()

data = pd.read_csv("$output_file", sep=' ')
data['Real_Time'] = data['Real_Time'].apply(convert_time)
data['User_Time'] = data['User_Time'].apply(convert_time)
data['Sys_Time'] = data['Sys_Time'].apply(convert_time)

# Individual plots
plot_and_save(data, 'Real_Time', 'Real Time', './real_time.png')
plot_and_save(data, 'User_Time', 'User Time', './user_time.png')
plot_and_save(data, 'Sys_Time', 'Sys Time', './sys_time.png')

# Combined plot
plt.figure(figsize=(10, 6))
plt.plot(data['N_Threads'], data['Real_Time'], label='Real Time', marker='o')
plt.plot(data['N_Threads'], data['User_Time'], label='User Time', marker='o')
plt.plot(data['N_Threads'], data['Sys_Time'], label='Sys Time', marker='o')
plt.title('Combined Performance Analysis')
plt.xlabel('Number of Threads')
plt.ylabel('Time (seconds)')
plt.legend()
plt.grid(True)
plt.savefig('./combined_graph.png')
plt.show()
END
