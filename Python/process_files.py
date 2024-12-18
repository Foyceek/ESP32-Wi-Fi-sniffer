import os
from glob import glob
from combine_pcap import combine_pcaps
from relevant_data import extract_probe_requests
from anonymize import anonymize_csv
from rssi_ranges import plot_rssi_distribution
from packets_over_time import plot_packet_count
from mac_type import plot_mac_address_types
from ssid_targets import plot_ssid_groups

def process_files(input_dir, output_dir, selected_options, quick_dir_var, anonymize=True, progress_callback=None,
                   file_label_callback=None, table_callback=None,
                   rssi_tab=None, packet_count_tab=None, mac_address_tab=None, ssid_tab=None, set_progress_max_callback=None):
    """Processes .pcap files in the input directory based on the selected options.

    Args:
        input_dir (str): Directory containing .pcap files.
        output_dir (str): Directory to save the output files.
        selected_options (list): Selected analysis options.
        anonymize (bool): Whether to anonymize the output CSV.
        progress_callback (callable, optional): Function to update progress bar.
        file_label_callback (callable, optional): Function to update file processing label.
    """
    try:
        if not quick_dir_var:
            # List all .pcap files in the input directory
            file_pattern = os.path.join(input_dir, "file_*.pcap")
            pcap_files = sorted(glob(file_pattern))
            total_files = len(pcap_files)

            if not pcap_files:
                raise FileNotFoundError("No .pcap files found in the selected directory.")

            # Adjust progress steps to include two extra steps
            total_steps = total_files + 3

            # Set the progress bar maximum dynamically
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

            # Update "Finished" after the last file
            if file_label_callback:
                file_label_callback("Finished combining")

            if progress_callback:
                progress_callback(total_files)  # Increment progress for "Finished combining"

            # Generate relevant_data.csv
            relevant_data_csv = f"{output_dir}/relevant_data.csv"
            ie_summary, total_probes = extract_probe_requests(
                pcap_file=f"{output_dir}/combined_output.pcap",
                output_csv=relevant_data_csv,
                only_create_table=quick_dir_var
            )

            # Optionally anonymize relevant_data.csv
            if anonymize:
                anonymized_csv = f"{output_dir}/anonymized_relevant_data.csv"
                anonymize_csv(relevant_data_csv, anonymized_csv)

            # Update the table in the GUI with the extracted data
            if table_callback:
                table_callback(ie_summary, total_probes)

            if file_label_callback:
                file_label_callback("Finished extracting relevant data")

            if progress_callback:
                progress_callback(total_steps)  # Increment progress for "Finished extracting relevant data"
        else:
            total_steps = 1
            # Set the progress bar maximum dynamically
            if set_progress_max_callback:
                set_progress_max_callback(total_steps)

        if 'RSSI Ranges' in selected_options:
            plot_rssi_distribution(f"{output_dir}/anonymized_relevant_data.csv", rssi_tab)
        if 'Packets Over Time' in selected_options:
            plot_packet_count(f"{output_dir}/anonymized_relevant_data.csv", packet_count_tab, time_resolution='15min')
        if 'MAC Address Types' in selected_options:
            plot_mac_address_types(f"{output_dir}/anonymized_relevant_data.csv", mac_address_tab)
        if 'SSID Targets' in selected_options:
            plot_ssid_groups(f"{output_dir}/anonymized_relevant_data.csv", ssid_tab)

        if file_label_callback:
            file_label_callback("Analysis done")

        if progress_callback:
            progress_callback(total_steps)  # Increment progress for "Analysis done"

    except Exception as e:
        raise RuntimeError(f"Error during processing: {e}")
