import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.cm import viridis
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import re

# Set common plot theme for all plots
def apply_common_theme():
    fontsize = 14
    fontsize_title = fontsize + 4
    fontsize_label = fontsize + 2
    plt.rcParams.update({
        'axes.titlesize': fontsize_title,
        'axes.labelsize': fontsize_label,
        'xtick.labelsize': fontsize,
        'ytick.labelsize': fontsize,
        'legend.fontsize': fontsize-2,
        'figure.figsize': (10, 6)
    })

# Utility function to identify heartbeat packets
def is_heartbeat(row):
    """Check if a row represents a heartbeat packet"""
    # Strip whitespace from MAC address and RSSI for proper comparison
    mac_clean = str(row['MAC']).strip()
    rssi_clean = str(row['RSSI']).strip()
    
    return (mac_clean == '00:00:00:00:00:00' and 
            rssi_clean == '-1 [HEARTBEAT]')


def plot_packet_count(csv_file, plot_tab, time_resolution, save_figure, output_dir=None):
    
    def plot():
        # Clear existing plots
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Read CSV file into a DataFrame
        df = pd.read_csv(csv_file)
        
        # Clean whitespace from all string columns
        string_columns = df.select_dtypes(include=['object']).columns
        for col in string_columns:
            df[col] = df[col].astype(str).str.strip()
        
        print(f"[DEBUG] Packet Count - Loaded CSV with {len(df)} rows and {len(df.columns)} columns")
        print(f"[DEBUG] Packet Count - Columns: {list(df.columns)}")

        # Combine Date and Time columns into a single datetime column
        df['Datetime'] = pd.to_datetime(df['DATE'] + ' ' + df['TIME'])
        print(f"[DEBUG] Packet Count - Date range: {df['Datetime'].min()} to {df['Datetime'].max()}")

        # Identify heartbeat packets
        df['is_heartbeat'] = df.apply(is_heartbeat, axis=1)
        heartbeat_count = df['is_heartbeat'].sum()
        regular_packet_count = len(df) - heartbeat_count
        print(f"[DEBUG] Packet Count - Heartbeat packets: {heartbeat_count}")
        print(f"[DEBUG] Packet Count - Regular packets: {regular_packet_count}")

        # Set Datetime as the index
        df.set_index('Datetime', inplace=True)

        # Convert time_resolution to an integer
        numeric_resolution = int(re.findall(r'\d+', time_resolution)[0]) if isinstance(time_resolution, str) else time_resolution
        print(f"[DEBUG] Packet Count - Time resolution: {time_resolution} (numeric: {numeric_resolution})")

        # Resample data by the specified time resolution - this includes ALL packets (heartbeats + regular)
        total_resampled = df.resample(time_resolution).size()
        
        # Resample only regular packets (non-heartbeats) for the main plot
        regular_packets_df = df[~df['is_heartbeat']]
        regular_resampled = regular_packets_df.resample(time_resolution).size()
        
        # Resample only heartbeats to track sniffer activity
        heartbeat_df = df[df['is_heartbeat']]
        heartbeat_resampled = heartbeat_df.resample(time_resolution).size()
        
        print(f"[DEBUG] Packet Count - Total resampled bins: {len(total_resampled)}")
        print(f"[DEBUG] Packet Count - Regular packet count range: {regular_resampled.min()} to {regular_resampled.max()}")
        print(f"[DEBUG] Packet Count - Heartbeat bins with activity: {(heartbeat_resampled > 0).sum()}")

        # Apply common theme
        apply_common_theme()

        # Create the figure and axis
        fig, ax = plt.subplots()

        # Extract time and values for regular packets
        times = regular_resampled.index
        values = regular_resampled.values

        zero_segments = 0
        normal_segments = 0
        gap_durations = []

        i = 0
        while i < len(times) - 1:
            if values[i] == 0:
                start_idx = i
                while i < len(times) and values[i] == 0:
                    i += 1
                end_idx = i - 1
                
                gap_duration = (times[end_idx] - times[start_idx]).total_seconds() / 60
                gap_durations.append(gap_duration)
                
                # Check if there were heartbeats during this zero period
                # This helps distinguish between "sniffer off" vs "no devices"
                zero_period_start = times[start_idx]
                zero_period_end = times[end_idx]
                
                # Check in the original heartbeat dataframe for any heartbeats in this time range
                heartbeats_in_period = heartbeat_df.loc[zero_period_start:zero_period_end]
                heartbeat_count_in_period = len(heartbeats_in_period)
                
                if heartbeat_count_in_period > 0:
                    # There were heartbeats, so sniffer was active but no devices detected
                    linestyle = 'dashed'
                    color = 'orange'
                    print(f"[DEBUG] Packet Count - Gap with heartbeats from {zero_period_start} to {zero_period_end}: {heartbeat_count_in_period} heartbeats")
                else:
                    # No heartbeats, likely sniffer was off
                    linestyle = 'solid'
                    color = 'r'
                
                ax.plot([times[start_idx], times[end_idx]], [0, 0], color=color, linestyle=linestyle)
                zero_segments += 1
                
                if end_idx + 1 < len(times) and values[end_idx + 1] > 0:
                    ax.plot([times[end_idx], times[end_idx + 1]], [0, values[end_idx + 1]], 'b')
            else:
                ax.plot([times[i], times[i + 1]], [values[i], values[i + 1]], 'b')
                normal_segments += 1
                i += 1

        print(f"[DEBUG] Packet Count - Zero segments: {zero_segments}, Normal segments: {normal_segments}")
        if gap_durations:
            print(f"[DEBUG] Packet Count - Gap durations (min): min={min(gap_durations):.2f}, max={max(gap_durations):.2f}, avg={np.mean(gap_durations):.2f}")

        ax.set_title('Regular Packet Count Over Time')
        ax.set_xlabel('Time')
        ax.set_ylabel('Packet Count')
        ax.grid(True)

        # Add legend for line styles
        from matplotlib.lines import Line2D
        legend_elements = [
            Line2D([0], [0], color='b', linestyle='solid', label='Regular packets'),
            Line2D([0], [0], color='orange', linestyle='dashed', label='No packets (sniffer active)'),
            Line2D([0], [0], color='r', linestyle='solid', label='No packets (sniffer possibly off)')
        ]
        ax.legend(handles=legend_elements, loc='upper right')

        # Embed the plot into the Tkinter tab
        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        # Add the toolbar to the canvas
        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'packet_count.png'))
            print(f"[DEBUG] Packet Count - Figure saved to {os.path.join(output_dir, 'packet_count.png')}")

    # Schedule the plot function on the main thread
    plot_tab.after(0, plot)

def plot_rssi_distribution(csv_file, plot_tab, save_figure, output_dir=None):
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Load the CSV file into a DataFrame
        df = pd.read_csv(csv_file)
        
        # Clean whitespace from all string columns
        string_columns = df.select_dtypes(include=['object']).columns
        for col in string_columns:
            df[col] = df[col].astype(str).str.strip()
        
        print(f"[DEBUG] RSSI Distribution - Loaded CSV with {len(df)} rows")
        
        # Check if RSSI column exists
        if 'RSSI' not in df.columns:
            print(f"[DEBUG] RSSI Distribution - ERROR: RSSI column not found. Available columns: {list(df.columns)}")
            return
        
        # Filter out heartbeat packets
        df['is_heartbeat'] = df.apply(is_heartbeat, axis=1)
        heartbeat_count = df['is_heartbeat'].sum()
        df_filtered = df[~df['is_heartbeat']]
        
        print(f"[DEBUG] RSSI Distribution - Total packets: {len(df)}")
        print(f"[DEBUG] RSSI Distribution - Heartbeat packets excluded: {heartbeat_count}")
        print(f"[DEBUG] RSSI Distribution - Regular packets for analysis: {len(df_filtered)}")
        
        if len(df_filtered) == 0:
            print(f"[DEBUG] RSSI Distribution - ERROR: No regular packets found after filtering heartbeats")
            return
        
        # Convert RSSI to numeric, handling any string formatting issues
        df_filtered['RSSI_numeric'] = pd.to_numeric(df_filtered['RSSI'], errors='coerce')
        df_filtered = df_filtered.dropna(subset=['RSSI_numeric'])
        
        print(f"[DEBUG] RSSI Distribution - RSSI column statistics:")
        print(f"[DEBUG] RSSI Distribution - Non-null RSSI values: {df_filtered['RSSI_numeric'].count()}")
        print(f"[DEBUG] RSSI Distribution - RSSI range: {df_filtered['RSSI_numeric'].min()} to {df_filtered['RSSI_numeric'].max()}")
        print(f"[DEBUG] RSSI Distribution - RSSI mean: {df_filtered['RSSI_numeric'].mean():.2f}")

        # Count occurrences of each RSSI value
        rssi_counts = df_filtered['RSSI_numeric'].value_counts().sort_index()
        print(f"[DEBUG] RSSI Distribution - Unique RSSI values: {len(rssi_counts)}")
        print(f"[DEBUG] RSSI Distribution - Most common RSSI values:")
        for rssi_val, count in rssi_counts.nlargest(5).items():
            print(f"[DEBUG] RSSI Distribution -   RSSI {rssi_val}: {count} occurrences")

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
        ax.set_xlabel('RSSI [dBm]')
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

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'rssi_distribution.png'))
            print(f"[DEBUG] RSSI Distribution - Figure saved to {os.path.join(output_dir, 'rssi_distribution.png')}")

    # Schedule the plot function on the main thread
    plot_tab.after(0, plot)

def plot_mac_address_types(csv_file, plot_tab, save_figure, output_dir=None):
    def is_randomized(mac_address):
        first_byte = int(mac_address.split(':')[0], 16)
        return (first_byte & 0x02) != 0

    def plot():
        for widget in plot_tab.winfo_children():
            widget.destroy()

        df = pd.read_csv(csv_file)
        
        # Clean whitespace from all string columns
        string_columns = df.select_dtypes(include=['object']).columns
        for col in string_columns:
            df[col] = df[col].astype(str).str.strip()
        
        print(f"[DEBUG] MAC Address Types - Loaded CSV with {len(df)} rows")
        
        # Check if MAC column exists
        if 'MAC' not in df.columns:
            print(f"[DEBUG] MAC Address Types - ERROR: MAC column not found. Available columns: {list(df.columns)}")
            return
        
        # Filter out heartbeat packets
        df['is_heartbeat'] = df.apply(is_heartbeat, axis=1)
        heartbeat_count = df['is_heartbeat'].sum()
        df_filtered = df[~df['is_heartbeat']]
        
        print(f"[DEBUG] MAC Address Types - Total packets: {len(df)}")
        print(f"[DEBUG] MAC Address Types - Heartbeat packets excluded: {heartbeat_count}")
        print(f"[DEBUG] MAC Address Types - Regular packets for analysis: {len(df_filtered)}")
        print(f"[DEBUG] MAC Address Types - Non-null MAC addresses: {df_filtered['MAC'].count()}")
        print(f"[DEBUG] MAC Address Types - Unique MAC addresses: {df_filtered['MAC'].nunique()}")

        if len(df_filtered) == 0:
            print(f"[DEBUG] MAC Address Types - ERROR: No regular packets found after filtering heartbeats")
            return

        # Apply MAC address type classification to filtered data
        df_filtered = df_filtered.copy()  # Avoid SettingWithCopyWarning
        df_filtered['MAC type'] = df_filtered['MAC'].apply(is_randomized)
        unique_count = df_filtered['MAC type'].value_counts()
        
        print(f"[DEBUG] MAC Address Types - Randomized MAC count: {unique_count.get(True, 0)}")
        print(f"[DEBUG] MAC Address Types - Globally Unique MAC count: {unique_count.get(False, 0)}")

        apply_common_theme()
        fig, ax = plt.subplots()
        unique_count.plot(kind='bar', color=['blue', 'red'], ax=ax)
        ax.set_title('Randomized vs. Globally Unique MAC Addresses')
        ax.set_ylabel('MAC Count')
        ax.set_xticks([0, 1])
        ax.set_xticklabels(['Globally Unique', 'Randomized'], rotation=0)

        for i, v in enumerate(unique_count):
            ax.text(i, v + max(unique_count) * 0.01, str(v), ha='center', va='bottom', fontsize=10)

        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'mac_types.png'))
            print(f"[DEBUG] MAC Address Types - Figure saved to {os.path.join(output_dir, 'mac_types.png')}")

    plot_tab.after(0, plot)

def plot_ssid_groups(csv_file, plot_tab, save_figure, output_dir=None):
    def plot():
        for widget in plot_tab.winfo_children():
            widget.destroy()

        df = pd.read_csv(csv_file)
        print(f"[DEBUG] SSID Groups - Loaded CSV with {len(df)} rows")
        
        # Check if SSID column exists
        if 'SSID' not in df.columns:
            print(f"[DEBUG] SSID Groups - ERROR: SSID column not found. Available columns: {list(df.columns)}")
            return
        
        print(f"[DEBUG] SSID Groups - Non-null SSID values: {df['SSID'].count()}")
        print(f"[DEBUG] SSID Groups - Unique SSID values: {df['SSID'].nunique()}")
        
        # Count wildcard vs non-wildcard
        wildcard_count = (df['SSID'] == 'Wildcard').sum()
        non_wildcard_count = (df['SSID'] != 'Wildcard').sum()
        print(f"[DEBUG] SSID Groups - Wildcard entries: {wildcard_count}")
        print(f"[DEBUG] SSID Groups - Non-wildcard entries: {non_wildcard_count}")

        df['SSID Group'] = df['SSID'].apply(lambda x: 'Wildcard' if x == 'Wildcard' else 'Targeted')
        ssid_count = df['SSID Group'].value_counts()
        print(f"[DEBUG] SSID Groups - Final grouping: {dict(ssid_count)}")

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

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'ssid_groups.png'))
            print(f"[DEBUG] SSID Groups - Figure saved to {os.path.join(output_dir, 'ssid_groups.png')}")

    plot_tab.after(0, plot)

def plot_cdf_with_percentiles(csv_file, plot_tab, save_figure, output_dir=None):
    def plot():
        data = pd.read_csv(csv_file)
        print(f"[DEBUG] CDF - Loaded CSV with {len(data)} rows")
        print(f"[DEBUG] CDF - Columns: {list(data.columns)}")
        
        # Check required columns
        required_cols = ['DATE', 'TIME', 'MAC']
        missing_cols = [col for col in required_cols if col not in data.columns]
        if missing_cols:
            print(f"[DEBUG] CDF - ERROR: Missing required columns: {missing_cols}")
            return

        data['Timestamp'] = data['DATE'] + ' ' + data['TIME']
        data['Timestamp'] = pd.to_datetime(data['Timestamp'], format='%Y-%m-%d %H:%M:%S.%f')
        print(f"[DEBUG] CDF - Timestamp range: {data['Timestamp'].min()} to {data['Timestamp'].max()}")
        
        data = data.sort_values(by=['MAC', 'Timestamp'])
        print(f"[DEBUG] CDF - Unique MAC addresses: {data['MAC'].nunique()}")
        
        data['Time_diff'] = data.groupby('MAC')['Timestamp'].diff().dt.total_seconds()
        print(f"[DEBUG] CDF - Time differences calculated for {data['Time_diff'].count()} pairs")
        print(f"[DEBUG] CDF - Time diff range: {data['Time_diff'].min():.6f}s to {data['Time_diff'].max():.6f}s")
        
        data_filtered = data[data['Time_diff'] <= 1].dropna(subset=['Time_diff'])
        print(f"[DEBUG] CDF - Filtered to {len(data_filtered)} time differences <= 1 second")
        print(f"[DEBUG] CDF - Filtered time diff range: {data_filtered['Time_diff'].min():.6f}s to {data_filtered['Time_diff'].max():.6f}s")

        if len(data_filtered) == 0:
            print(f"[DEBUG] CDF - WARNING: No time differences <= 1 second found!")
            return

        apply_common_theme()
        fig, ax = plt.subplots()

        sorted_diff = np.sort(data_filtered['Time_diff'])
        cdf = np.arange(1, len(sorted_diff) + 1) / len(sorted_diff)

        # Plot CDF with better styling
        ax.plot(sorted_diff, cdf, marker='.', linestyle='-', alpha=0.7, markersize=2, linewidth=1)

        # Calculate percentiles (minimum is useless - always 0.001s due to timestamp resolution)
        percentiles = [25, 50, 75, 90, 95, 99]
        percentile_values = np.percentile(sorted_diff, percentiles)
        percentile_colors = ['purple', 'green', 'orange', 'red', 'darkred', 'black']
        
        print(f"[DEBUG] CDF - Percentile values:")
        for i, (p, val, color) in enumerate(zip(percentiles, percentile_values, percentile_colors)):
            print(f"[DEBUG] CDF -   {p}th percentile: {val:.6f}s")
            cdf_position = p / 100.0
            ax.scatter(val, cdf_position, color=color, zorder=5, 
                      label=f'{p}th: {val:.4f}s', marker='o', s=50, edgecolor='white', linewidth=1)
            
            # Add vertical line for better visibility
            ax.axvline(val, color=color, linestyle='--', alpha=0.3, linewidth=1)

        # Improved labeling and formatting
        ax.set_xlabel('Time Difference [s]')
        ax.set_ylabel('CDF [-]')
        ax.set_title('CDF of Time Differences Between Consecutive Arrivals')
        ax.grid(True, alpha=0.3)
        ax.legend(bbox_to_anchor=(0.98, 0.02), loc='lower right')
        
        # Set axis limits for better visibility
        ax.set_xlim(left=0, right=max(sorted_diff) * 1.05)
        ax.set_ylim(0, 1)

        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'cdf.png'))
            print(f"[DEBUG] CDF - Figure saved to {os.path.join(output_dir, 'cdf.png')}")

    plot_tab.after(0, plot)

def plot_device_detections(relevant_csv, devices_csv, plot_tab, time_resolution, hours_apart, save_figure=False, output_dir=None):
    def plot():
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Load full relevant data
        df_full = pd.read_csv(relevant_csv)
        df_full['Datetime'] = pd.to_datetime(df_full['DATE'] + ' ' + df_full['TIME'])
        df_full.set_index('Datetime', inplace=False)

        # Prepare packet count from all data (not filtered!)
        df_packet = df_full.set_index('Datetime')
        numeric_resolution = int(re.findall(r'\d+', time_resolution)[0]) if isinstance(time_resolution, str) else time_resolution
        resampled_df = df_packet.resample(time_resolution).size()
        times = resampled_df.index
        values = resampled_df.values

        # Load devices and filter to only valid multi-MAC entries
        devices_df = pd.read_csv(devices_csv)
        print(f"[DEBUG] Device Detections - Loaded devices CSV with {len(devices_df)} rows")

        # Ensure 'MACs' field is valid and not empty
        devices_df = devices_df[devices_df['MACs'].notna()]
        devices_df = devices_df[devices_df['MACs'].astype(str).str.strip() != '']
        
        # Split MACs and clean them properly
        def parse_macs(mac_string):
            """Parse MAC addresses from string, handling quotes and whitespace"""
            if pd.isna(mac_string):
                return []
            
            # Convert to string and strip outer quotes if present
            mac_str = str(mac_string).strip()
            if mac_str.startswith('"') and mac_str.endswith('"'):
                mac_str = mac_str[1:-1]
            
            # Split by comma and clean each MAC
            macs = []
            for mac in mac_str.split(','):
                cleaned_mac = mac.strip().lower()
                if cleaned_mac and len(cleaned_mac) >= 12:  # Basic MAC length check
                    macs.append(cleaned_mac)
            
            return macs

        devices_df['MAC_list'] = devices_df['MACs'].apply(parse_macs)

        # Keep only devices with strictly more than 1 valid MAC address
        multi_mac_devices = devices_df[devices_df['MAC_list'].apply(len) > 1].copy()

        # Print debugging information
        print(f"[DEBUG] Device Detections - Total devices in CSV: {len(devices_df)}")
        print(f"[DEBUG] Device Detections - Devices with valid MACs field: {len(devices_df[devices_df['MACs'].notna()])}")
        print(f"[DEBUG] Device Detections - Found {len(multi_mac_devices)} valid multi-MAC devices (devices with >1 MAC).")

        # If no multi-MAC devices found, show warning and return
        if len(multi_mac_devices) == 0:
            print("[DEBUG] Device Detections - WARNING: No multi-MAC devices found! Nothing to plot.")
            # Create empty plot with warning message
            apply_common_theme()
            fig, ax = plt.subplots()
            ax.text(0.5, 0.5, 'No multi-MAC devices found in the dataset', 
                   ha='center', va='center', transform=ax.transAxes, fontsize=16)
            ax.set_title("Device Occurrences")
            
            canvas = FigureCanvasTkAgg(fig, master=plot_tab)
            canvas.draw()
            toolbar = NavigationToolbar2Tk(canvas, plot_tab)
            toolbar.update()
            canvas.get_tk_widget().pack(fill="both", expand=True)
            toolbar.pack(side="top", fill="x")
            return

        # Map MAC to device ID (using consecutive numbering starting from 1)
        mac_to_device = {}
        for device_num, (idx, row) in enumerate(multi_mac_devices.iterrows(), start=1):
            device_id = device_num  # Consecutive device ID starting from 1
            for mac in row['MAC_list']:
                mac_to_device[mac] = device_id

        # Filter only relevant rows that match multi-MAC devices
        df_full['MAC_lower'] = df_full['MAC'].astype(str).str.strip().str.lower()
        df_full['DeviceID'] = df_full['MAC_lower'].map(mac_to_device)
        filtered_df = df_full.dropna(subset=['DeviceID']).copy()
        filtered_df['DeviceID'] = filtered_df['DeviceID'].astype(int)
        grouped_df = filtered_df[['Datetime', 'DeviceID']].drop_duplicates()
        all_valid_macs = set(mac for macs in multi_mac_devices['MAC_list'] for mac in macs)

        print(f"[DEBUG] Device Detections - Number of unique MACs across valid multi-MAC devices: {len(all_valid_macs)}")
        print(f"[DEBUG] Device Detections - Total packets from multi-MAC devices: {len(filtered_df)}")
        print(f"[DEBUG] Device Detections - Unique DeviceIDs being plotted (before gap filter): {filtered_df['DeviceID'].nunique()}")

        # NEW: Filter devices to only include those with occurrences more than 1 hour apart
        def has_hour_plus_gap(device_data):
            if len(device_data) < 2:
                return False
            
            # Sort by datetime
            sorted_times = device_data['Datetime'].sort_values()
            
            # Check consecutive time differences
            for i in range(1, len(sorted_times)):
                time_diff = sorted_times.iloc[i] - sorted_times.iloc[i-1]
                if time_diff.total_seconds() > int(hours_apart)*3600:
                    return True
            return False

        valid_device_ids = []
        for device_id in grouped_df['DeviceID'].unique():
            device_data = grouped_df[grouped_df['DeviceID'] == device_id]
            if has_hour_plus_gap(device_data):
                valid_device_ids.append(device_id)

        print(f"[DEBUG] Device Detections - Devices with 4+ hour gaps: {len(valid_device_ids)} out of {grouped_df['DeviceID'].nunique()}")

        # Create a mapping from original device IDs to consecutive plot IDs (1, 2, 3, ...)
        original_to_plot_id = {orig_id: plot_id for plot_id, orig_id in enumerate(sorted(valid_device_ids), start=1)}
        plot_to_original_id = {plot_id: orig_id for orig_id, plot_id in original_to_plot_id.items()}

        # Filter grouped_df and filtered_df to only include valid devices
        grouped_df = grouped_df[grouped_df['DeviceID'].isin(valid_device_ids)]
        filtered_df = filtered_df[filtered_df['DeviceID'].isin(valid_device_ids)]
        
        # Remap device IDs to consecutive plot IDs
        grouped_df['PlotDeviceID'] = grouped_df['DeviceID'].map(original_to_plot_id)
        filtered_df['PlotDeviceID'] = filtered_df['DeviceID'].map(original_to_plot_id)

        # If no devices meet the criteria, show warning and return
        if len(valid_device_ids) == 0:
            print("[DEBUG] Device Detections - WARNING: No devices found with occurrences more than 4 hours apart!")
            # Create empty plot with warning message
            apply_common_theme()
            fig, ax = plt.subplots()
            ax.text(0.5, 0.5, 'No devices found with occurrences more than 4 hours apart', 
                   ha='center', va='center', transform=ax.transAxes, fontsize=16)
            ax.set_title("Device Occurrences")
            
            canvas = FigureCanvasTkAgg(fig, master=plot_tab)
            canvas.draw()
            toolbar = NavigationToolbar2Tk(canvas, plot_tab)
            toolbar.update()
            canvas.get_tk_widget().pack(fill="both", expand=True)
            toolbar.pack(side="top", fill="x")
            return

        print(f"[DEBUG] Device Detections - Final DeviceIDs being plotted: {len(valid_device_ids)}")

        # Apply common theme for consistent aspect ratio
        apply_common_theme()
        
        # Create figure and axes
        fig, ax1 = plt.subplots()

        # Plot packet count (left axis)
        i = 0
        while i < len(times) - 1:
            if values[i] == 0:
                start_idx = i
                while i < len(times) and values[i] == 0:
                    i += 1
                end_idx = i - 1
                gap_duration = (times[end_idx] - times[start_idx]).total_seconds() / 60
                linestyle = 'dotted' if gap_duration > 30 else 'solid'
                ax1.plot([times[start_idx], times[end_idx]], [0, 0], color='r', linestyle=linestyle)
                if end_idx + 1 < len(times):
                    ax1.plot([times[end_idx], times[end_idx + 1]], [0, values[end_idx + 1]], 'b')
            else:
                ax1.plot([times[i], times[i + 1]], [values[i], values[i + 1]], 'b')
                i += 1

        ax1.set_xlabel("Time")
        ax1.set_ylabel("Packet Count", color='blue')
        ax1.tick_params(axis='y', labelcolor='blue')

        # Add time resolution label
        ax1.text(0.01, 0.95, f'Time resolution: {time_resolution}', transform=ax1.transAxes, fontsize=10,
                 verticalalignment='top', bbox=dict(facecolor='white', alpha=0.6, edgecolor='gray'))

        # Plot device occurrences (right axis, styled like lines)
        ax2 = ax1.twinx()
        colors = plt.cm.tab20.colors
        plot_device_ids = sorted(original_to_plot_id.values())  # 1, 2, 3, ..., 21

        for plot_id in plot_device_ids:
            original_id = plot_to_original_id[plot_id]
            device_data = grouped_df[grouped_df['DeviceID'] == original_id].sort_values('Datetime')
            ax2.plot(device_data['Datetime'], [plot_id] * len(device_data),
                     marker='o', linestyle='-', linewidth=1.5,
                     color=colors[(plot_id - 1) % len(colors)], label=f'Device {plot_id} (orig {original_id})')

        ax2.set_ylabel("Device ID", color='black')
        ax2.tick_params(axis='y', labelcolor='black')

        # FIXED: Set up y-axis ticks for consecutive device IDs (1, 2, 3, ..., 21)
        min_plot_id = 1
        max_plot_id = len(valid_device_ids)
        
        # For small numbers of devices, show reasonable tick spacing
        if max_plot_id <= 10:
            custom_ticks = list(range(1, max_plot_id + 1))  # Show all: 1, 2, 3, ..., max
        elif max_plot_id <= 25:
            custom_ticks = list(range(1, max_plot_id + 1, 2))  # Every 2nd: 1, 3, 5, ...
            if custom_ticks[-1] != max_plot_id:
                custom_ticks.append(max_plot_id)
        else:
            # For many devices, use nice spacing
            step = max(1, max_plot_id // 8)
            custom_ticks = list(range(1, max_plot_id + 1, step))
            if custom_ticks[-1] != max_plot_id:
                custom_ticks.append(max_plot_id)
            
        ax2.set_yticks(custom_ticks)
        
        device_range = max_plot_id - 1
        base_padding = 0.8
        percentage_padding = max(0.05 * device_range, 0.3)
        total_padding = base_padding + percentage_padding
        
        ax2_bottom = 1 - total_padding
        ax2_top = max_plot_id + total_padding
        
        # Align the axes so packet_count=0 aligns with device_id=1
        ax1_bottom, ax1_top = ax1.get_ylim()
        if ax1_bottom <= 0 <= ax1_top:
            zero_ratio = (0 - ax1_bottom) / (ax1_top - ax1_bottom)
            ax2_range = ax2_top - ax2_bottom
            ax2_aligned_bottom = 1 - zero_ratio * ax2_range
            ax2_aligned_top = ax2_aligned_bottom + ax2_range
            ax2.set_ylim(bottom=ax2_aligned_bottom, top=ax2_aligned_top)
        else:
            ax2.set_ylim(bottom=ax2_bottom, top=ax2_top)

        ax1.set_title("Device Occurrences")
        ax1.grid(True)

        # Plot in GUI
        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()
        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()
        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'device_occurrences.png'))
            print(f"[DEBUG] Device Detections - Figure saved to {os.path.join(output_dir, 'device_occurrences.png')}")

    plot_tab.after(0, plot)

def plot_battery_data(csv_file, plot_tab, save_figure, output_dir=None):
    def plot():
        # Clear existing plots
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Read CSV file into a DataFrame
        df = pd.read_csv(csv_file, names=['timestamp', 'voltage_mv', 'current_ma'])
        print(f"[DEBUG] Battery Data - Loaded CSV with {len(df)} rows and {len(df.columns)} columns")
        print(f"[DEBUG] Battery Data - Columns: {list(df.columns)}")

        # Convert timestamp to datetime (same approach as packet plot)
        df['timestamp'] = pd.to_datetime(df['timestamp'])
        print(f"[DEBUG] Battery Data - Date range: {df['timestamp'].min()} to {df['timestamp'].max()}")

        # Calculate duration for debug info
        duration_minutes = (df['timestamp'].iloc[-1] - df['timestamp'].iloc[0]).total_seconds() / 60
        print(f"[DEBUG] Battery Data - Duration: {duration_minutes:.1f} minutes")
        print(f"[DEBUG] Battery Data - Voltage range: {df['voltage_mv'].min():.0f} - {df['voltage_mv'].max():.0f} mV")
        print(f"[DEBUG] Battery Data - Current range: {df['current_ma'].min():.0f} - {df['current_ma'].max():.0f} mA")

        # Apply common theme
        apply_common_theme()

        # Create the figure with dual y-axes
        fig, ax1 = plt.subplots()

        # Plot voltage (left y-axis) using datetime timestamps
        color1 = 'tab:blue'
        ax1.plot(df['timestamp'], df['voltage_mv'], color=color1, linewidth=1.5, label='Voltage')
        ax1.set_xlabel('Time')
        ax1.set_ylabel('Voltage [mV]', color=color1)
        ax1.tick_params(axis='y', labelcolor=color1)
        ax1.grid(True, alpha=0.3)

        # Plot current (right y-axis) using datetime timestamps
        ax2 = ax1.twinx()
        color2 = 'tab:red'
        ax2.plot(df['timestamp'], df['current_ma'], color=color2, linewidth=1.5, label='Current')
        ax2.set_ylabel('Current [mA]', color=color2)
        ax2.tick_params(axis='y', labelcolor=color2)

        # Format x-axis to match packet plot style
        # This will automatically format datetime labels similar to the packet plot
        fig.autofmt_xdate()  # Rotate and format datetime labels

        # Add title
        ax1.set_title('Battery Data: Voltage and Current over Time')

        print(f"[DEBUG] Battery Data - Plot created with {len(df)} data points")

        # Embed the plot into the Tkinter tab
        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()

        # Add the toolbar to the canvas
        toolbar = NavigationToolbar2Tk(canvas, plot_tab)
        toolbar.update()

        canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar.pack(side="top", fill="x")

        if output_dir and save_figure:
            fig.savefig(os.path.join(output_dir, 'battery_data.png'))
            print(f"[DEBUG] Battery Data - Figure saved to {os.path.join(output_dir, 'battery_data.png')}")

    # Schedule the plot function on the main thread
    plot_tab.after(0, plot)