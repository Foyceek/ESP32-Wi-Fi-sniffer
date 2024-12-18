import csv
from scapy.all import rdpcap, Dot11, Dot11Elt
from collections import defaultdict, Counter
import string
from datetime import datetime

def extract_probe_requests(pcap_file, output_csv, only_create_table):
    # Read the pcap file
    packets = rdpcap(pcap_file)

    # Dictionary to store devices and their preferred SSIDs
    devices = defaultdict(list)

    # Counter for Information Elements
    ie_counter = Counter()

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

                # Extract Date and Time
                date = timestamp.strftime('%Y-%m-%d')
                time = timestamp.strftime('%H:%M:%S')

                # Extract RSSI (if available in the packet)
                rssi = packet.dBm_AntSignal if hasattr(packet, 'dBm_AntSignal') else "N/A"

                # Store the data (if not only creating the table)
                if not only_create_table:
                    devices[mac_address].append([date, time, mac_address, ssid, rssi])

                # Extract and count Information Elements (IEs)
                if packet.haslayer(Dot11Elt):
                    elts = packet.getlayer(Dot11Elt)
                    processed_ie_ids = set()  # To avoid double counting within the same packet
                    while isinstance(elts, Dot11Elt):
                        if elts.ID not in processed_ie_ids:
                            ie_counter[elts.ID] += 1
                            processed_ie_ids.add(elts.ID)
                        elts = elts.payload if isinstance(elts.payload, Dot11Elt) else None

    # If not only creating the table, write the results to a CSV file
    if not only_create_table:
        with open(output_csv, mode='w', newline='', encoding='utf-8') as csv_file:
            writer = csv.writer(csv_file)

            # Write CSV header
            writer.writerow(["Date", "Time", "Device MAC", "Preferred SSIDs", "RSSI"])

            # Write device data
            for data in devices.values():
                for entry in data:
                    writer.writerow(entry)

    # Prepare summary table data
    total_probes = len(unique_probes)
    ie_summary = []
    ie_names = {
        1: "Supported rates",
        50: "Extended Supported rates",
        45: "HT Capabilities",
        191: "VHT Capabilities",
        127: "Extended Capabilities",
        59: "HE Capabilities",
        68: "WPS - UUID-E",
        2: "WEP Protected"
    }

    for ie_id, name in ie_names.items():
        count = ie_counter.get(ie_id, 0)
        percentage = (count / total_probes) * 100 if total_probes > 0 else 0.0
        ie_summary.append([name, count, f"{percentage:.2f}"])

    # print("Done")

    return ie_summary, total_probes  # Return the summary

# Example usage
# extract_probe_requests(
#     pcap_file=r"C:\\Franta\\VUT\\5_semestr\\SEP\\analize_output\\combined_output.pcap",
#     output_csv=r"C:\\Franta\\VUT\\5_semestr\\SEP\\analize_output\\relevant_data.csv",
#     only_create_table=False
# )