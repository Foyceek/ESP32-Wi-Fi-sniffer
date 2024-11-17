import csv
from scapy.all import rdpcap
from scapy.layers.dot11 import Dot11, RadioTap
from datetime import datetime, timedelta, timezone

# Filepath for the pcap file
pcap_file = r'C:\Franta\VUT\5_semestr\probe_analyzer\Python\source_data_prep\B07_13-11-24.pcap'
output_file = "source_mac_addresses_with_signal_strength.csv"

# Read the pcap file
packets = rdpcap(pcap_file)

# Initialize a list to store timestamp, MAC addresses, and signal strength
data = []

# Iterate through the packets
for packet in packets:
    # Check if the packet has both Radiotap and Dot11 layers
    if RadioTap in packet and Dot11 in packet:
        timestamp = packet.time  # Get the timestamp (Unix time)
        mac_address = packet[Dot11].addr2  # Extract the source MAC address
        # Extract the signal strength from the Radiotap layer (dBm)
        signal_strength = packet[RadioTap].dBm_AntSignal if hasattr(packet[RadioTap], 'dBm_AntSignal') else None
        
        # Convert Unix timestamp to human-readable format
        timestamp_float = float(timestamp)  # Convert timestamp to float if it's not already
        readable_timestamp = datetime.fromtimestamp(timestamp_float, timezone.utc) + timedelta(hours=1)  # Adjust for 1-hour offset
        readable_timestamp = readable_timestamp.strftime('%a %b %d %H:%M:%S %Y')

        # Ensure all values are valid and append to the data list
        if mac_address and signal_strength is not None:
            data.append([readable_timestamp, mac_address, signal_strength])

# Write the extracted data to a CSV file
with open(output_file, mode="w", newline="") as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(["Timestamp", "MAC Address", "Signal Strength"])  # Write the header
    writer.writerows(data)  # Write all rows of data

# Print the results
print(f"Extracted {len(data)} packets with relevant information.")
print(f"Data saved to {output_file}.")
