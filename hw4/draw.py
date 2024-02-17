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

data = pd.read_csv("timing_results.txt", sep=' ')
data['Real_Time'] = data['Real_Time'].apply(convert_time)
data['User_Time'] = data['User_Time'].apply(convert_time)
data['Sys_Time'] = data['Sys_Time'].apply(convert_time)
data['Ratio'] = (data['User_Time'] + data['Sys_Time']) / data['Real_Time']

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
plt.close()

plt.figure(figsize=(10, 6))
plt.plot(data['N_Threads'], data['Ratio'], label='(User Time + Sys Time) / Real Time', marker='o')
plt.title('(User Time + Sys Time) / Real Time')
plt.xlabel('Number of Threads')
plt.ylabel('Ratio')
plt.ylim(bottom=0, top=8)

# Mark the number of threads on each data point
for i, txt in enumerate(data['N_Threads']):
    plt.text(data['N_Threads'][i], data['Ratio'][i], f'{txt}', ha='center', va='bottom')

plt.legend()
plt.grid(True)
plt.savefig('./relation.png')
plt.close()

