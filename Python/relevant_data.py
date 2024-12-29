import csv
from scapy.all import rdpcap, Dot11
import string
from datetime import datetime

def extract_probe_requests(pcap_file, output_csv):
    # Read the pcap file
    packets = rdpcap(pcap_file)

    # List to store all packet data with timestamps
    all_entries = []

    # Track unique probe requests to avoid duplicate counting
    unique_probes = set()

    # Process each packet
    for packet in packets:
        if packet.haslayer(Dot11):
            dot11 = packet[Dot11]

            # Check if it's a probe request
            if dot11.type == 0 and dot11.subtype == 4:
                # Extract device MAC address
                mac_address = dot11.addr2
                if not mac_address:
                    continue

                # Extract Datetime and use it to create a unique identifier for the packet
                timestamp = datetime.fromtimestamp(float(packet.time))
                unique_id = (mac_address, timestamp)

                if unique_id in unique_probes:
                    continue
                unique_probes.add(unique_id)

                # Extract preferred SSID (if any)
                ssid = "Wildcard"
                try:
                    if hasattr(packet, 'info'):
                        ssid = packet.info.decode('utf-8', errors='ignore')
                        if not ssid or not all(ch in string.printable for ch in ssid):
                            ssid = "Wildcard"
                except Exception:
                    pass

                # Extract Date and Time with microsecond precision
                date = timestamp.strftime('%Y-%m-%d')
                time = timestamp.strftime('%H:%M:%S') + f".{timestamp.microsecond:06d}"

                # Extract RSSI (if available in the packet)
                rssi = packet.dBm_AntSignal if hasattr(packet, 'dBm_AntSignal') else "N/A"

                # Append the data to the list
                all_entries.append([date, time, mac_address, ssid, rssi, timestamp])

    # Sort entries by the timestamp (6th column)
    all_entries.sort(key=lambda x: x[5])

    # Write the results to a CSV file
    with open(output_csv, mode='w', newline='', encoding='utf-8') as csv_file:
        writer = csv.writer(csv_file)

        # Write CSV header
        writer.writerow(["Date", "Time", "Device MAC", "Preferred SSIDs", "RSSI"])

        # Write sorted device data
        for entry in all_entries:
            writer.writerow(entry[:-1])  # Exclude the timestamp column from the output
