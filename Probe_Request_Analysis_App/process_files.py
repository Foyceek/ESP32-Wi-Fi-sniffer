import os
import tkinter as tk
from tkinter import messagebox

from glob import glob
from combine_pcap import combine_pcaps
from relevant_data import extract_probe_requests
from create_table import create_probe_requests_table
from anonymize import anonymize_csv
from reduced_match import rewrite_csv
from instances import extract_instances
from devices import extract_devices

from plotting import plot_packet_count, plot_rssi_distribution, plot_mac_address_types
from plotting import plot_ssid_groups, plot_cdf_with_percentiles, plot_device_detections, plot_battery_data


def check_required_files(input_dir, output_dir, quick_dir_var, anonymize, reduced_analysis, create_table, selected_options):
    """
    Checks if the required files are present based on analysis configuration.
    """
    required_input_files = []
    required_output_files = []
    
    battery_selected = 'Battery' in selected_options
    non_battery_selected = any(opt != 'Battery' for opt in selected_options)

    # Battery analysis requirements
    if battery_selected:
        target_dir = output_dir if quick_dir_var else input_dir
        target_list = required_output_files if quick_dir_var else required_input_files
        target_list.append("BATTERY_DATA.csv")

    # Non-battery analysis requirements
    if non_battery_selected:
        if quick_dir_var:
            required_output_files.append("combined_output.pcap")
        elif reduced_analysis:
            required_input_files.append("REDUCED_DATA.csv")
            if create_table:
                required_output_files.append("combined_output.pcap")
            if anonymize:
                required_output_files.append("relevant_data.csv")
        else:
            required_input_files.append("file_000000.pcap")

    # Check for missing files
    missing_input_files = [f for f in required_input_files if not os.path.isfile(os.path.join(input_dir, f))]
    missing_output_files = [f for f in required_output_files if not os.path.isfile(os.path.join(output_dir, f))]

    if missing_input_files or missing_output_files:
        message_parts = []
        if missing_input_files:
            message_parts.append("Input Directory:\n" + "\n".join(missing_input_files))
        if missing_output_files:
            message_parts.append("Output Directory:\n" + "\n".join(missing_output_files))
        
        missing_files_message = "The following required files are missing:\n\n" + "\n\n".join(message_parts)
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("Missing Files", missing_files_message)
        return False

    return True


def update_progress_and_file_label(progress_callback, file_label_callback, progress_val, label_text):
    """Helper function to update progress and file label."""
    if progress_callback:
        progress_callback(progress_val)
    if file_label_callback:
        file_label_callback(label_text)


def process_pcap_files(input_dir, output_dir, progress_callback, file_label_callback, set_progress_max_callback):
    """Process PCAP files from input directory."""
    file_pattern = os.path.join(input_dir, "file_*.pcap")
    pcap_files = sorted(glob(file_pattern))
    
    if not pcap_files:
        raise FileNotFoundError("No .pcap files found in the selected directory.")
    
    total_files = len(pcap_files)
    total_steps = total_files + 3
    
    if set_progress_max_callback:
        set_progress_max_callback(total_steps)

    # Combine PCAP files
    combine_pcaps(
        pcap_files,
        output_file=f"{output_dir}/combined_output.pcap",
        progress_callback=lambda count, file_name: (
            update_progress_and_file_label(progress_callback, file_label_callback, 
                                          count, f"Processing: {file_name} ({count}/{total_files})")
        )
    )

    update_progress_and_file_label(progress_callback, file_label_callback, 
                                   total_files, "Finished combining")
    
    return total_steps

def extract_and_process_data(output_dir, anonymize):
    """Extract relevant data and process instances/devices."""
    relevant_data_csv = f"{output_dir}/relevant_data.csv"
    extract_probe_requests(f"{output_dir}/combined_output.pcap", relevant_data_csv)

    instances_csv = f"{output_dir}/instances.csv"
    
    if anonymize:
        anonymized_csv = f"{output_dir}/anonymized_relevant_data.csv"
        anonymize_csv(relevant_data_csv, anonymized_csv)
        extract_instances(anonymized_csv, instances_csv)
    else:
        extract_instances(relevant_data_csv, instances_csv)
    
    # Extract devices
    devices_csv = f"{output_dir}/devices.csv"
    extract_devices(instances_csv, devices_csv, threshold=0.5)


def get_data_file_path(output_dir, quick_dir_var, reduced_analysis, anonymize):
    """Get the appropriate data file path based on analysis type."""
    if quick_dir_var:
        return f"{output_dir}/anonymized_relevant_data.csv" if anonymize else f"{output_dir}/relevant_data.csv"
    elif reduced_analysis:
        return f"{output_dir}/REDUCED_DATA_MATCH.csv"
    else:
        return f"{output_dir}/anonymized_relevant_data.csv" if anonymize else f"{output_dir}/relevant_data.csv"


def plot_selected_data(data_file, selected_options, output_dir, reduced_analysis, time_resolution, hours_apart, save_figure,
                       packet_count_tab, rssi_tab, mac_address_tab, ssid_tab, cdf_tab, devices_tab):
    """Plot data based on selected options."""
    plot_functions = {
        'Packets Over Time': lambda: plot_packet_count(data_file, packet_count_tab, time_resolution, save_figure, output_dir),
        'RSSI Ranges': lambda: plot_rssi_distribution(data_file, rssi_tab, save_figure, output_dir),
        'MAC Address Types': lambda: plot_mac_address_types(data_file, mac_address_tab, save_figure, output_dir),
    }
    
    # These plots are not available for reduced analysis
    if not reduced_analysis:
        plot_functions.update({
            'SSID Targets': lambda: plot_ssid_groups(data_file, ssid_tab, save_figure, output_dir),
            'CDF': lambda: plot_cdf_with_percentiles(data_file, cdf_tab, save_figure, output_dir),
            'Devices': lambda: plot_device_detections(data_file, f"{output_dir}/devices.csv", devices_tab, time_resolution, hours_apart, save_figure, output_dir)
        })
    
    for option in selected_options:
        if option in plot_functions:
            plot_functions[option]()


def process_files(input_dir, output_dir, selected_options, time_resolution, hours_apart, quick_dir_var, anonymize, reduced_analysis,
                  create_table, save_figure, progress_callback=None, file_label_callback=None, table_callback=None,
                  rssi_tab=None, packet_count_tab=None, mac_address_tab=None, ssid_tab=None, cdf_tab=None, 
                  devices_tab=None, battery_tab=None, set_progress_max_callback=None):
    """
    Processes files based on the selected analysis options.
    """
    # Check for required files
    if not check_required_files(input_dir, output_dir, quick_dir_var, anonymize, reduced_analysis, create_table, selected_options):
        return

    update_progress_and_file_label(progress_callback, file_label_callback, 0, "Analysis started")

    try:
        battery_selected = 'Battery' in selected_options
        non_battery_selected = any(opt != 'Battery' for opt in selected_options)
        total_steps = 1

        # Process non-battery analysis
        if non_battery_selected:
            if quick_dir_var:
                if set_progress_max_callback:
                    set_progress_max_callback(total_steps)
            elif reduced_analysis:
                if set_progress_max_callback:
                    set_progress_max_callback(total_steps)
                rewrite_csv(f"{input_dir}/REDUCED_DATA.csv", f"{output_dir}/REDUCED_DATA_MATCH.csv")
            else:
                total_steps = process_pcap_files(input_dir, output_dir, progress_callback, file_label_callback, set_progress_max_callback)
                extract_and_process_data(output_dir, anonymize)
                update_progress_and_file_label(progress_callback, file_label_callback, 
                                               None, "Finished extracting relevant data")
        else:
            if set_progress_max_callback:
                set_progress_max_callback(total_steps)

        # Create table if requested
        if create_table and non_battery_selected:
            ie_summary, total_probes = create_probe_requests_table(f"{output_dir}/relevant_data.csv")
            if table_callback:
                table_callback(ie_summary, total_probes)

        # Handle battery analysis
        if battery_selected:
            battery_csv = f"{output_dir if quick_dir_var else input_dir}/BATTERY_DATA.csv"
            plot_battery_data(battery_csv, battery_tab, save_figure, output_dir)

        # Plot non-battery data
        if non_battery_selected:
            data_file = get_data_file_path(output_dir, quick_dir_var, reduced_analysis, anonymize)
            non_battery_options = [opt for opt in selected_options if opt != 'Battery']
            plot_selected_data(data_file, non_battery_options, output_dir, reduced_analysis, time_resolution, hours_apart,
                              save_figure, packet_count_tab, rssi_tab, mac_address_tab, ssid_tab, cdf_tab, devices_tab)

        update_progress_and_file_label(progress_callback, file_label_callback, 
                                       total_steps, "Analysis done")
        print("Analysis done")
        
    except Exception as e:
        raise RuntimeError(f"Error during processing: {e}")