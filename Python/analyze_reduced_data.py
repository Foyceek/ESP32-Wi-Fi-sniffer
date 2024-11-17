import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import csv
from collections import Counter
from datetime import timedelta

# Load the data from CSV
data = pd.read_csv("REDUCED_DATA.csv", header=None, names=["datetime", "mac", "rssi"])

# ----------------------------
# Plot 1: Number of Entries per 15-Minute Block
# ----------------------------
# Parse datetime column
data["datetime"] = pd.to_datetime(data["datetime"], format='%a %b %d %H:%M:%S %Y')

# Subtract one hour from each datetime entry
data["datetime"] = data["datetime"] - timedelta(hours=1)

# Set datetime as index
data.set_index("datetime", inplace=True)

# Resample the data into 15-minute intervals, counting the number of entries in each interval
data_resampled = data.resample("15min").size()

# Calculate the start and end date
start_date = data_resampled.index.min().normalize()
end_date = data_resampled.index.max().normalize() + timedelta(days=1) - timedelta(seconds=1)

# Plotting the result
plt.figure(figsize=(12, 6))
plt.plot(data_resampled.index, data_resampled.values, color="green", marker='o', markersize=3)
plt.xlabel("Time")
plt.ylabel("Number of Entries")
plt.title("Number of Entries per 15-Minute Block")

# Set x-axis format to show dates
plt.gca().xaxis.set_major_locator(mdates.DayLocator())
plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))

# Add alternating background color for each day
current_date = start_date
color_flag = True
while current_date < end_date:
    next_day = current_date + timedelta(days=1)
    plt.axvspan(current_date, next_day, color='gray' if color_flag else 'white', alpha=0.1)
    color_flag = not color_flag
    current_date = next_day

# Add vertical lines for each midnight
for single_date in pd.date_range(start_date, end_date + timedelta(days=1), freq='D'):
    plt.axvline(single_date, color='blue', linestyle='--', linewidth=0.7)

# Add vertical lines for each noon
for single_date in pd.date_range(start_date, end_date, freq='D'):
    noon_time = single_date + timedelta(hours=12)
    plt.axvline(noon_time, color='red', linestyle='--', linewidth=0.7)

plt.xticks(rotation=45)
plt.tight_layout()

# ----------------------------
# Plot 2: RSSI Strength vs. Number of Entries
# ----------------------------
# Extract RSSI values
rssi_values = data["rssi"].astype(int)

# Count the frequency of each RSSI value
rssi_counts = Counter(rssi_values)
sorted_rssi = sorted(rssi_counts.items())
rssi = [item[0] for item in sorted_rssi]
counts = [item[1] for item in sorted_rssi]

# Apply the viridis colormap to the RSSI values
cmap = plt.cm.viridis
norm = plt.Normalize(min(rssi), max(rssi))

plt.figure(figsize=(10, 6))
bars = plt.bar(rssi, counts, width=1, align='center', color=cmap(norm(rssi)))
plt.xlabel('RSSI (dBm)', fontsize=12)
plt.ylabel('Number of Entries', fontsize=12)
plt.title('RSSI Strength vs. Number of Entries', fontsize=14)
plt.grid(True)

# ----------------------------
# Plot 3: Top MAC Addresses by Number of Entries
# ----------------------------
# Settings
num_top_macs = 20
entry_threshold = 10
highlight_macs = [
    'B0:22:7A:F7:31:73',
    'FC:B3:BC:CA:4E:B4',
    'FC:B3:BC:CA:4E:B8',
    '00:FF:58:56:2F:FD',
    '00:FF:BD:F3:4E:CA',
    '1C:F8:D0:6D:EA:53'
]

# Count MAC address entries
mac_count = Counter(data["mac"].str.replace('-', ':').str.strip())
filtered_macs = [(mac, count) for mac, count in mac_count.items() if count >= entry_threshold]
filtered_macs.sort(key=lambda x: x[1], reverse=True)
top_macs = filtered_macs[:num_top_macs]

# Prepare data for plotting
macs = [item[0] for item in top_macs]
counts = [item[1] for item in top_macs]

plt.figure(figsize=(10, 6))
bars = plt.barh(macs, counts, color='black')

# Highlight specific MAC addresses
highlighted_indexes = [i for i, mac in enumerate(macs) if mac in highlight_macs]
for index in highlighted_indexes:
    bars[index].set_color('red')

plt.xlabel('Number of Entries')
plt.ylabel('MAC Addresses')
plt.title(f'Top {num_top_macs} MAC Addresses by Number of Entries (Threshold: {entry_threshold})')

# Show all plots
plt.show()
