import os
from glob import glob
from combine_pcap import combine_pcaps
from relevant_data import extract_probe_requests
from create_table import create_probe_requests_table
from anonymize import anonymize_csv
from reduced_match import rewrite_csv

from plotting import plot_packet_count
from plotting import plot_rssi_distribution
from plotting import plot_mac_address_types
from plotting import plot_ssid_groups
from plotting import plot_cdf_with_percentiles

import tkinter as tk
from tkinter import messagebox

def check_required_files(input_dir, output_dir, quick_dir_var, anonymize, reduced_analysis, create_table):
    """
    Checks if the required files are present in the input and output directories 
    based on the combination of quick_dir_var, anonymize, reduced_analysis, and create_table.

    Args:
        input_dir (str): Input directory path.
        output_dir (str): Output directory path.
        quick_dir_var (bool): Whether quick directory processing is enabled.
        anonymize (bool): Whether anonymization is enabled.
        reduced_analysis (bool): Whether reduced analysis is enabled.
        create_table (bool): Whether table creation is enabled.

    Returns:
        bool: True if all required files are present, False otherwise.
    """
    required_input_files = []
    required_output_files = []

    # Define required files based on conditions
    if not quick_dir_var:
        required_input_files.append("file_000000.pcap")  # Example file for non-quick processing
    else:
        required_output_files.append("combined_output.pcap")  # Output after combining

    if reduced_analysis:
        required_input_files.append("REDUCED_DATA.csv")
        if create_table:
            required_output_files.append("combined_output.pcap")  # Used to create tables
        if anonymize:
            required_output_files.append("relevant_data.csv")

    # Check for missing files
    missing_input_files = [f for f in required_input_files if not os.path.isfile(os.path.join(input_dir, f))]
    missing_output_files = [f for f in required_output_files if not os.path.isfile(os.path.join(output_dir, f))]

    if missing_input_files or missing_output_files:
        missing_files_message = (
            "The following required files are missing:\n\n"
            + "Input Directory:\n" + "\n".join(missing_input_files) + "\n\n"
            + "Output Directory:\n" + "\n".join(missing_output_files)
        )
        root = tk.Tk()
        root.withdraw()  # Hide the root window
        messagebox.showerror("Missing Files", missing_files_message)
        return False

    return True


def process_files(input_dir, output_dir, selected_options, time_resolution, quick_dir_var, anonymize, reduced_analysis,
                  create_table, progress_callback=None,
                  file_label_callback=None, table_callback=None,
                  rssi_tab=None, packet_count_tab=None, mac_address_tab=None, ssid_tab=None, cdf_tab=None,
                  set_progress_max_callback=None):
    """
    Processes .pcap files in the input directory based on the selected options.

    Args:
        input_dir (str): Directory containing .pcap files.
        output_dir (str): Directory to save the output files.
        selected_options (list): Selected analysis options.
        anonymize (bool): Whether to anonymize the output CSV.
        progress_callback (callable, optional): Function to update progress bar.
        file_label_callback (callable, optional): Function to update file processing label.
    """

    # Check for required files based on conditions
    if not check_required_files(input_dir, output_dir, quick_dir_var, anonymize, reduced_analysis, create_table):
        return  # Stop execution if files are missing

    if file_label_callback:
        file_label_callback("Analysis started")

    if progress_callback:
        progress_callback(0)

    try:
        # Process files as per existing logic
        if not quick_dir_var and not reduced_analysis:
            # List and process .pcap files
            file_pattern = os.path.join(input_dir, "file_*.pcap")
            pcap_files = sorted(glob(file_pattern))
            total_files = len(pcap_files)

            if not pcap_files:
                raise FileNotFoundError("No .pcap files found in the selected directory.")

            total_steps = total_files + 3  # Including the extra steps
            if set_progress_max_callback:
                set_progress_max_callback(total_steps)

            # Combine .pcap files
            combine_pcaps(
                pcap_files,
                output_file=f"{output_dir}/combined_output.pcap",
                progress_callback=lambda count, file_name: (
                    progress_callback(count) if progress_callback else None,
                    file_label_callback(f"Processing: {file_name} ({count}/{total_files})") if file_label_callback else None
                )
            )

            if file_label_callback:
                file_label_callback("Finished combining")
            if progress_callback:
                progress_callback(total_files)

            # Extract relevant data
            relevant_data_csv = f"{output_dir}/relevant_data.csv"
            extract_probe_requests(f"{output_dir}/combined_output.pcap", relevant_data_csv)

            if anonymize:
                anonymized_csv = f"{output_dir}/anonymized_relevant_data.csv"
                anonymize_csv(relevant_data_csv, anonymized_csv)

            if file_label_callback:
                file_label_callback("Finished extracting relevant data")
        else:
            total_steps = 1
            if set_progress_max_callback:
                set_progress_max_callback(total_steps)

        if reduced_analysis:
            rewrite_csv(f"{input_dir}/REDUCED_DATA.csv", f"{output_dir}/REDUCED_DATA_MATCH.csv")

        if create_table:
            # Generate the probe requests table
            ie_summary, total_probes = create_probe_requests_table(f"{output_dir}/combined_output.pcap")
            if table_callback:
                table_callback(ie_summary, total_probes)

        # Helper function to plot data
        def plot_selected_data(data_file):
            # Plot data based on selected options
            if 'Packets Over Time' in selected_options:
                plot_packet_count(data_file, packet_count_tab, time_resolution)
            if 'RSSI Ranges' in selected_options:
                plot_rssi_distribution(data_file, rssi_tab)
            if 'MAC Address Types' in selected_options:
                plot_mac_address_types(data_file, mac_address_tab)
            if not reduced_analysis and 'SSID Targets' in selected_options:
                plot_ssid_groups(data_file, ssid_tab)
            if not reduced_analysis and 'CDF' in selected_options:
                plot_cdf_with_percentiles(data_file, cdf_tab)

        # Plot data based on selected options
        if anonymize:
            plot_selected_data(f"{output_dir}/anonymized_relevant_data.csv")
        elif reduced_analysis:
            plot_selected_data(f"{output_dir}/REDUCED_DATA_MATCH.csv")
        else:
            plot_selected_data(f"{output_dir}/relevant_data.csv")

        if file_label_callback:
            file_label_callback("Analysis done")

        if progress_callback:
            progress_callback(total_steps)  # Final progress update

    except Exception as e:
        raise RuntimeError(f"Error during processing: {e}")
