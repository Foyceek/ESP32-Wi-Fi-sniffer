import os
from glob import glob
from scapy.all import rdpcap, wrpcap

pcap_dir = r"C:\Franta\VUT\5_semestr\probe_analyzer\night_capture"  # Update to your actual directory path
file_pattern = os.path.join(pcap_dir, "file_*.pcap")
output_file = "concatenated_output.pcap"

all_packets = []
last_timestamp = 0

# Debugging
print(f"Listing files in directory: {pcap_dir}")
files_in_dir = os.listdir(pcap_dir)
print(f"Files in directory: {files_in_dir}")

# Use glob to find all .pcap files that match the pattern
pcap_files = sorted(glob(file_pattern))

# Debugging
print(f"Files found by glob: {pcap_files}")

if pcap_files:
    for file_path in pcap_files:
        print(f"Processing {file_path}")
        
        try:
            packets = rdpcap(file_path)
            
            # Skip empty .pcap files
            if not packets:
                print(f"Warning: {file_path} is empty or unreadable, skipping.")
                continue

            # Get the file's last modification time (timestamp in seconds)
            file_timestamp = os.path.getmtime(file_path)

            # Sync the last packet timestamp with the file's modification time
            if packets:
                last_packet_time = packets[-1].time
                timestamp_shift = file_timestamp - last_packet_time

                # Adjust the timestamps for all packets
                for packet in packets:
                    packet.time += timestamp_shift

            # Update the last timestamp
            if len(packets) > 0:
                last_timestamp = packets[-1].time

            # Add the packets to the final list
            all_packets.extend(packets)

        except Exception as e:
            print(f"Error reading {file_path}: {e}")

    # Write the concatenated packets to a new file
    if all_packets:
        wrpcap(output_file, all_packets)
        print(f"Concatenation complete. Output file: {output_file}")
    else:
        print("No packets to process.")
else:
    print("No .pcap files found.")
