import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.cm import viridis
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk

# Set common plot theme for all plots
def apply_common_theme():
    plt.rcParams.update({
        'axes.titlesize': 14,
        'axes.labelsize': 12,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.figsize': (10, 6)
    })

# Function to plot packet count over time
def plot_packet_count(csv_file, plot_tab, time_resolution):
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Read the CSV file into a DataFrame
        df = pd.read_csv(csv_file)

        # Combine Date and Time columns into a single datetime column
        df['Datetime'] = pd.to_datetime(df['Date'] + ' ' + df['Time'])

        # Set Datetime as the index for easy resampling
        df.set_index('Datetime', inplace=True)

        # Resample the data by the specified time resolution
        resampled_df = df.resample(time_resolution).size()

        # Apply common theme
        apply_common_theme()

        # Create the figure and axis explicitly
        fig, ax = plt.subplots()

        # Plot the packet count over time on the axis
        ax.plot(resampled_df.index, resampled_df.values)
        ax.set_title('Packet Count Over Time')
        ax.set_xlabel('Time')
        ax.set_ylabel('Packet Count')
        ax.grid(True)

        # Embed the plot into the Tkinter tab
        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        # Add the toolbar to the canvas
        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

    # Schedule the plot function on the main thread
    plot_tab.after(0, plot)

# Function to plot RSSI distribution
def plot_rssi_distribution(csv_file, plot_tab):
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Load the CSV file into a DataFrame
        df = pd.read_csv(csv_file)

        # Count occurrences of each RSSI value
        rssi_counts = df['RSSI'].value_counts().sort_index()

        # Normalize counts for color mapping
        norm = plt.Normalize(vmin=rssi_counts.index.min(), vmax=rssi_counts.index.max())
        colors = viridis(norm(rssi_counts.index))

        # Apply common theme
        apply_common_theme()

        # Create the bar plot
        fig, ax = plt.subplots()
        ax.bar(rssi_counts.index, rssi_counts.values, color=colors, width=1.0, edgecolor='black')

        # Customize the plot
        ax.set_title('RSSI Value Distribution')
        ax.set_xlabel('RSSI')
        ax.set_ylabel('RSSI Count')
        ax.grid(axis='y', linestyle='--', alpha=0.7)

        # Embed the plot into the Tkinter tab
        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        # Add the toolbar to the canvas
        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

    # Schedule the plot function on the main thread
    plot_tab.after(0, plot)

# Function to plot MAC address types
def plot_mac_address_types(csv_file, plot_tab):
    def is_randomized(mac_address):
        first_byte = int(mac_address.split(':')[0], 16)
        return (first_byte & 0x02) != 0

    def plot():
        for widget in plot_tab.winfo_children():
            widget.destroy()

        df = pd.read_csv(csv_file)
        df['MAC type'] = df['Device MAC'].apply(is_randomized)
        unique_count = df['MAC type'].value_counts()

        apply_common_theme()
        fig, ax = plt.subplots()
        unique_count.plot(kind='bar', color=['blue', 'red'], ax=ax)
        ax.set_title('Randomized vs. Globally Unique MAC Addresses')
        ax.set_ylabel('MAC Count')
        ax.set_xticks([0, 1])
        ax.set_xticklabels(['Globally Unique', 'Randomized'], rotation=0)

        for i, v in enumerate(unique_count):
            ax.text(i, v + 10, str(v), ha='center', va='bottom', fontsize=10)

        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

    plot_tab.after(0, plot)

# Function to plot SSID groups (Wildcard vs Targeted)
def plot_ssid_groups(csv_file, plot_tab):
    def plot():
        for widget in plot_tab.winfo_children():
            widget.destroy()

        df = pd.read_csv(csv_file)
        df['SSID Group'] = df['Preferred SSIDs'].apply(lambda x: 'Wildcard' if x == 'Wildcard' else 'Targeted')
        ssid_count = df['SSID Group'].value_counts()

        apply_common_theme()
        fig, ax = plt.subplots()
        ssid_count.plot(kind='bar', color=['blue', 'red'], ax=ax)
        ax.set_title('Packets Grouped by SSID Type (Wildcard vs. Targeted)')
        ax.set_ylabel('Packet Count')
        ax.set_xticks([0, 1])
        ax.set_xticklabels(['Wildcard (Non-targeted)', 'Targeted'], rotation=0)

        for i, v in enumerate(ssid_count):
            ax.text(i, v + 10, str(v), ha='center', va='bottom', fontsize=10)

        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

    plot_tab.after(0, plot)

# Function to plot CDF with percentiles
def plot_cdf_with_percentiles(csv_file, plot_tab):
    def plot():
        data = pd.read_csv(csv_file)
        data['Timestamp'] = data['Date'] + ' ' + data['Time']
        data['Timestamp'] = pd.to_datetime(data['Timestamp'], format='%Y-%m-%d %H:%M:%S.%f')
        data = data.sort_values(by=['Device MAC', 'Timestamp'])
        data['Time_diff'] = data.groupby('Device MAC')['Timestamp'].diff().dt.total_seconds()
        data_filtered = data[data['Time_diff'] <= 1].dropna(subset=['Time_diff'])

        apply_common_theme()
        fig, ax = plt.subplots()

        sorted_diff = np.sort(data_filtered['Time_diff'])
        cdf = np.arange(1, len(sorted_diff) + 1) / len(sorted_diff)

        ax.plot(sorted_diff, cdf, marker='.', linestyle='none', color='black')

        min_time = np.min(sorted_diff)
        median_time = np.percentile(sorted_diff, 50)
        p75_time = np.percentile(sorted_diff, 75)
        p95_time = np.percentile(sorted_diff, 95)

        ax.scatter(min_time, 0, color='blue', zorder=5, label=f'Min {min_time:.6f}s', marker='x')
        ax.scatter(median_time, 0.5, color='green', zorder=5, label=f'Median {median_time:.6f}s', marker='x')
        ax.scatter(p75_time, 0.75, color='orange', zorder=5, label=f'75th Percentile {p75_time:.6f}s', marker='x')
        ax.scatter(p95_time, 0.95, color='red', zorder=5, label=f'95th Percentile {p95_time:.6f}s', marker='x')

        ax.set_xlabel('Time Difference (seconds)')
        ax.set_ylabel('CDF')
        ax.set_title('Cumulative Distribution Function of Time Differences Between Consecutive Arrivals')
        ax.grid(True)
        ax.legend()

        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

    plot_tab.after(0, plot)
