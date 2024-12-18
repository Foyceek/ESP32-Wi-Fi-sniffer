from scapy.all import rdpcap, wrpcap
import os

def combine_pcaps(pcap_files, output_file="concatenated_output.pcap", progress_callback=None):
    all_packets = []

    for count, file_path in enumerate(pcap_files, start=1):
        try:
            print(f"[DEBUG] Reading file: {file_path}")
            packets = rdpcap(file_path)

            # Skip empty files
            if not packets:
                print(f"[DEBUG] Warning: {file_path} is empty or unreadable, skipping.")
                continue

            all_packets.extend(packets)

        except Exception as e:
            print(f"[DEBUG] Error reading {file_path}: {e}")

        # Update progress
        if progress_callback:
            progress_callback(count, os.path.basename(file_path))

    # Write all packets to the output file
    if all_packets:
        wrpcap(output_file, all_packets)
        print(f"[DEBUG] Concatenation complete. Output file: {output_file}")
    else:
        print("[DEBUG] No packets to process.")
