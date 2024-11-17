import csv
from scapy.all import rdpcap
from scapy.layers.dot11 import Dot11

# Filepath for the pcap file
pcap_file = "B07_13-11-24.pcap"
output_file = "source_mac_addresses.csv"

# Read the pcap file
packets = rdpcap(pcap_file)

# Initialize a list to store all source MAC addresses
source_mac_addresses = []

# Iterate through the packets
for packet in packets:
    # Check if the packet has an 802.11 layer
    if Dot11 in packet:
        addr2 = packet[Dot11].addr2  # Extract the source MAC address
        if addr2:  # Ensure it's not None
            source_mac_addresses.append(addr2)

# Write all MAC addresses to a CSV file
with open(output_file, mode="w", newline="") as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(["Source MAC Address"])  # Write the header
    for mac in source_mac_addresses:
        writer.writerow([mac])  # Write each MAC address

print(f"Extracted {len(source_mac_addresses)} source MAC addresses.")
print(f"MAC addresses saved to {output_file}.")
