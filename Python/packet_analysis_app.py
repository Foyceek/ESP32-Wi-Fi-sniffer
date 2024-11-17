import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from collections import Counter
from datetime import timedelta
import tkinter as tk
from tkinter import filedialog, messagebox
from tkinter.ttk import Notebook
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import csv
from itertools import combinations
from tkinter import scrolledtext

def calculate_similarity(list1, list2):
    """Calculate the similarity percentage between two lists."""
    set1, set2 = set(list1), set(list2)
    intersection = set1 & set2
    return len(intersection) / max(len(set1), len(set2))

def group_devices_by_ssid_similarity(file_path, thresholds):
    """Group devices based on SSID similarity."""
    devices = []
    with open(file_path, 'r') as file:
        reader = csv.DictReader(file)
        # Strip any leading/trailing spaces from headers
        reader.fieldnames = [field.strip() for field in reader.fieldnames]
        for row in reader:
            mac = row["Device MAC"]
            ssids = [ssid.strip() for ssid in row["Preferred SSIDs"].split(",")]
            devices.append((mac, ssids))

    results = {}
    for threshold in thresholds:
        groups = []
        ungrouped = set(range(len(devices)))
        for i, j in combinations(range(len(devices)), 2):
            if i in ungrouped and j in ungrouped:
                similarity = calculate_similarity(devices[i][1], devices[j][1])
                if similarity >= threshold:
                    found_group = False
                    for group in groups:
                        if i in group or j in group:
                            group.update({i, j})
                            found_group = True
                            break
                    if not found_group:
                        groups.append({i, j})
                    ungrouped -= {i, j}

        for index in ungrouped:
            groups.append({index})

        filtered_groups = []
        for group in groups:
            mac_addresses = [devices[idx][0] for idx in group]
            if len(mac_addresses) >= 2 and len(set(mac_addresses)) >= 2:
                ssids = list(set(ssid for idx in group for ssid in devices[idx][1]))
                filtered_groups.append({
                    "devices": mac_addresses,
                    "ssids": ssids
                })
        results[threshold] = filtered_groups

    return results

def display_analysis_results(results, tab):
    """Display analysis results in a new tab in the GUI."""
    output_text = scrolledtext.ScrolledText(tab, wrap=tk.WORD, width=100, height=30)
    output_text.pack(padx=10, pady=10, fill=tk.BOTH, expand=True)

    for threshold, groups in results.items():
        output_text.insert(tk.END, f"\n### Analysis for Similarity Threshold: {threshold * 100}% ###\n")
        for idx, group in enumerate(groups, 1):
            output_text.insert(tk.END, f"Device({idx}):\n")
            output_text.insert(tk.END, f"  MAC Addresses: {', '.join(group['devices'])}\n")
            output_text.insert(tk.END, f"  Preferred SSIDs: {', '.join(group['ssids'])}\n")

    output_text.config(state=tk.DISABLED)  # Disable editing after displaying the text

def plot_device_counts(results, tab):
    """Plot the number of devices found at each threshold and display in a new tab."""
    thresholds = []
    device_counts = []

    for threshold, groups in results.items():
        thresholds.append(threshold * 100)  # Convert to percentage
        device_counts.append(len(groups))  # Number of groups

    # Generate the plot
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(thresholds, device_counts, marker='o', color='black', linestyle='-')
    ax.set_title("Devices Found vs. Similarity Threshold", fontsize=14)
    ax.set_xlabel("Similarity Threshold (%)", fontsize=12)
    ax.set_ylabel("Number of Devices Found", fontsize=12)
    ax.grid(True)
    
    fig.tight_layout()

    # Add the plot to a new tab
    canvas = FigureCanvasTkAgg(fig, master=tab)
    canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
    canvas.draw()

def load_devices_file():
    """Load the devices CSV file, perform analysis, and display results."""
    file_path = filedialog.askopenfilename(filetypes=[("CSV files", "*.csv")])
    if file_path:
        similarity_thresholds = [0.75, 0.80, 0.85, 0.90, 0.95]
        analysis_results = group_devices_by_ssid_similarity(file_path, similarity_thresholds)

        # Create a new tab to display results
        result_tab = tk.Frame(notebook)
        notebook.add(result_tab, text="Device Analysis Results")
        display_analysis_results(analysis_results, result_tab)

        # Create another tab for the devices plot
        plot_tab = tk.Frame(notebook)
        notebook.add(plot_tab, text="Devices Plot")
        plot_device_counts(analysis_results, plot_tab)

        status_label.config(text="Analysis Complete", bg='green')

def select_file():
    # Ask the user to select a file
    file_path = filedialog.askopenfilename(filetypes=[("CSV files", "*.csv")])
    if file_path:
        file_label.config(text=f"Selected File: {file_path}")
        global selected_file
        selected_file = file_path

def categorize_mac(mac):
    # Remove colons and trim spaces for consistency in comparison
    mac = mac.replace(":", "").strip().upper()  # Ensure it's uppercase and clean
    
    # Check if the MAC address is Locally Assigned (DA:A1:19) prefix
    if len(mac) == 12:  # Ensure the MAC address is in the correct format (12 hex characters)
        # Check for the DA:A1:19 prefix
        if mac.startswith("DAA119"):
            # print(f"Categorized as Locally Assigned (DA:A1:19): {mac}")  # For debugging
            return "Locally Assigned (DA:A1:19)"
        
        # Check if the MAC address is Locally Assigned based on second byte
        second_byte = mac[2]  # Get the second byte of the MAC address
        if second_byte in ['2', '6', 'A', 'E']:
            return "Locally Assigned"
    
    # If not, it's a Globally Unique MAC
    return "Globally Unique"

def analyze_file():
    if not selected_file:
        messagebox.showerror("Error", "No file selected!")
        return
    
    if not any([var_plot1.get(), var_plot2.get(), var_plot3.get(), var_plot4.get()]):
        messagebox.showwarning("No Plot Selected", "Please select at least one plot to analyze.")
        return

    # Update label to indicate "working" status (orange)
    status_label.config(text="Working on analysis...", bg='orange')
    root.update_idletasks()

    # Clear all tabs except the 'Setup', 'Device Analysis Results', and 'Devices Plot' tabs
    for tab in notebook.tabs():
        if notebook.tab(tab, "text") not in {"Setup", "Device Analysis Results", "Devices Plot"}:
            notebook.forget(tab)

    try:
        # Read CSV with proper headers and parse the Timestamp
        data = pd.read_csv(selected_file, names=["timestamp", "mac", "rssi"], header=0)

        # Convert 'timestamp' to datetime
        data["timestamp"] = pd.to_datetime(data["timestamp"], format='%a %b %d %H:%M:%S %Y')

        # ----------------------------
        # Plot 1: Number of Entries per 15-Minute Block
        # ----------------------------
        if var_plot1.get():
            data["timestamp"] = data["timestamp"] - timedelta(hours=1)  # Adjust time if needed
            data.set_index("timestamp", inplace=True)
            data_resampled = data.resample("15min").size()
            start_date = data_resampled.index.min().normalize()
            end_date = data_resampled.index.max().normalize() + timedelta(days=1) - timedelta(seconds=1)

            fig1, ax1 = plt.subplots(figsize=(12, 6))
            ax1.plot(data_resampled.index, data_resampled.values, color="green", marker='o', markersize=3)
            ax1.set_xlabel("Time")
            ax1.set_ylabel("Number of Entries")
            ax1.set_title("Number of Entries per 15-Minute Block")
            ax1.xaxis.set_major_locator(mdates.DayLocator())
            ax1.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
            current_date = start_date
            color_flag = True
            while current_date < end_date:
                next_day = current_date + timedelta(days=1)
                ax1.axvspan(current_date, next_day, color='gray' if color_flag else 'white', alpha=0.1)
                color_flag = not color_flag
                current_date = next_day
            for single_date in pd.date_range(start_date, end_date + timedelta(days=1), freq='D'):
                ax1.axvline(single_date, color='blue', linestyle='--', linewidth=0.7)
            for single_date in pd.date_range(start_date, end_date, freq='D'):
                noon_time = single_date + timedelta(hours=12)
                ax1.axvline(noon_time, color='red', linestyle='--', linewidth=0.7)

            fig1.tight_layout()  # Correct usage of tight_layout for figure
            # Create a new tab for Plot 1
            tab_name = "Entries per 15-Min"
            tab = tk.Frame(notebook)
            notebook.add(tab, text=tab_name)
            
            canvas1 = FigureCanvasTkAgg(fig1, master=tab)
            canvas1.get_tk_widget().pack(fill=tk.BOTH, expand=True)
            canvas1.draw()

            status_label.config(text="Generating RSSI plot...", bg='orange')

        # ----------------------------
        # Plot 2: RSSI Strength vs. Number of Entries
        # ----------------------------
        if var_plot2.get():
            rssi_values = data["rssi"].astype(int)
            rssi_counts = Counter(rssi_values)
            sorted_rssi = sorted(rssi_counts.items())
            rssi = [item[0] for item in sorted_rssi]
            counts = [item[1] for item in sorted_rssi]
            cmap = plt.cm.viridis
            norm = plt.Normalize(min(rssi), max(rssi))
            fig2, ax2 = plt.subplots(figsize=(10, 6))
            ax2.bar(rssi, counts, width=1, align='center', color=cmap(norm(rssi)))
            ax2.set_xlabel('RSSI (dBm)', fontsize=12)
            ax2.set_ylabel('Number of Entries', fontsize=12)
            ax2.set_title('RSSI Strength vs. Number of Entries', fontsize=14)
            ax2.grid(True)

            fig2.tight_layout()  # Correct usage of tight_layout for figure

            # Create a new tab for Plot 2
            tab_name = "RSSI Strength"
            tab = tk.Frame(notebook)
            notebook.add(tab, text=tab_name)
            
            canvas2 = FigureCanvasTkAgg(fig2, master=tab)
            canvas2.get_tk_widget().pack(fill=tk.BOTH, expand=True)
            canvas2.draw()

            status_label.config(text="Generating MAC Address plot...", bg='orange')

        # ----------------------------
        # Plot 3: Top MAC Addresses by Number of Entries
        # ----------------------------
        if var_plot3.get():
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
            mac_count = Counter(data["mac"].str.replace('-', ':').str.strip())
            filtered_macs = [(mac, count) for mac, count in mac_count.items() if count >= entry_threshold]
            filtered_macs.sort(key=lambda x: x[1], reverse=True)
            top_macs = filtered_macs[:num_top_macs]
            macs = [item[0] for item in top_macs]
            counts = [item[1] for item in top_macs]
            fig3, ax3 = plt.subplots(figsize=(10, 6))
            bars = ax3.barh(macs, counts, color='black')
            highlighted_indexes = [i for i, mac in enumerate(macs) if mac in highlight_macs]
            for index in highlighted_indexes:
                bars[index].set_color('red')
            ax3.set_xlabel('Number of Entries')
            ax3.set_ylabel('MAC Addresses')
            ax3.set_title(f'Top {num_top_macs} MAC Addresses by Number of Entries (Threshold: {entry_threshold})')

            fig3.tight_layout()  # Correct usage of tight_layout for figure

            # Create a new tab for Plot 3
            tab_name = "Top MAC Addresses"
            tab = tk.Frame(notebook)
            notebook.add(tab, text=tab_name)
            
            canvas3 = FigureCanvasTkAgg(fig3, master=tab)
            canvas3.get_tk_widget().pack(fill=tk.BOTH, expand=True)
            canvas3.draw()

            status_label.config(text="Generating MAC Categorization plot...", bg='orange')

        # ----------------------------
        # Plot 4: MAC Address Categorization
        # ----------------------------
        if var_plot4.get():
            data['Category'] = data['mac'].apply(categorize_mac)
            category_counts = data['Category'].value_counts()
            fig4, ax4 = plt.subplots(figsize=(8, 6))
            category_counts.plot(kind='bar', color=['blue', 'green', 'red'], ax=ax4)
            ax4.set_title('MAC Address Categorization')
            ax4.set_xlabel('Category')
            ax4.set_ylabel('Count')
            fig4.tight_layout()

            # Create a new tab for Plot 4
            tab_name = "MAC Categorization"
            tab = tk.Frame(notebook)
            notebook.add(tab, text=tab_name)
            
            canvas4 = FigureCanvasTkAgg(fig4, master=tab)
            canvas4.get_tk_widget().pack(fill=tk.BOTH, expand=True)
            canvas4.draw()

            status_label.config(text="Analysis Complete", bg='green')

    except Exception as e:
        messagebox.showerror("Error", f"An error occurred: {e}")
        status_label.config(text="Error occurred!", bg='red')

# Main Tkinter Setup
root = tk.Tk()
root.title("Probe Analyzer")

# Maximize the window
root.state('zoomed')

# Notebook for tabbed interface
notebook = Notebook(root)
notebook.pack(fill=tk.BOTH, expand=True)

# Setup Tab
setup_tab = tk.Frame(notebook)
notebook.add(setup_tab, text="Setup")

# File selection
file_label = tk.Label(setup_tab, text="No file selected", width=50, anchor="w")
file_label.pack(padx=10, pady=5)

# Buttons for file selection and analysis
select_button = tk.Button(setup_tab, text="Select CSV File", command=select_file)
select_button.pack(padx=10, pady=5)

load_devices_button = tk.Button(setup_tab, text="Load Devices CSV", command=load_devices_file)
load_devices_button.pack(padx=10, pady=5)

analyze_button = tk.Button(setup_tab, text="Analyze Data", command=analyze_file)
analyze_button.pack(padx=10, pady=5)


# Status label
status_label = tk.Label(setup_tab, text="Status: Ready", bg='lightgray', width=50, anchor="w")
status_label.pack(padx=10, pady=5)

# Checkboxes for plots
var_plot1 = tk.BooleanVar(value=True)
var_plot2 = tk.BooleanVar(value=True)
var_plot3 = tk.BooleanVar(value=True)
var_plot4 = tk.BooleanVar(value=True)

plot1_checkbox = tk.Checkbutton(setup_tab, text="Plot 1: Number of Entries per 15-Minute Block", variable=var_plot1)
plot1_checkbox.pack(padx=10, pady=5)

plot2_checkbox = tk.Checkbutton(setup_tab, text="Plot 2: RSSI Strength vs. Number of Entries", variable=var_plot2)
plot2_checkbox.pack(padx=10, pady=5)

plot3_checkbox = tk.Checkbutton(setup_tab, text="Plot 3: Top MAC Addresses by Number of Entries", variable=var_plot3)
plot3_checkbox.pack(padx=10, pady=5)

plot4_checkbox = tk.Checkbutton(setup_tab, text="Plot 4: MAC Address Categorization", variable=var_plot4)
plot4_checkbox.pack(padx=10, pady=5)


root.mainloop()