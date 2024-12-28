from datetime import datetime
from collections import Counter
from scapy.all import rdpcap, Dot11, Dot11Elt

def create_probe_requests_table(pcap_file):
    print("Creating table")
    # Read the pcap file
    packets = rdpcap(pcap_file)

    # Counter for Information Elements
    ie_counter = Counter()

    # Track unique probe requests to avoid duplicate counting
    unique_probes = set()

    # Track the number of Vendor Specific IEs in each packet
    vendor_specific_counts = Counter()

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

                # Extract and count Information Elements (IEs)
                if packet.haslayer(Dot11Elt):
                    elts = packet.getlayer(Dot11Elt)
                    processed_ie_ids = set()  # To avoid double counting within the same packet
                    vendor_specific_count = 0  # Count of Vendor Specific IEs in this packet
                    while isinstance(elts, Dot11Elt):
                        if elts.ID == 150 or elts.ID == 221:  # Vendor Specific IEs
                            vendor_specific_count += 1
                        if elts.ID not in processed_ie_ids:
                            ie_counter[elts.ID] += 1
                            processed_ie_ids.add(elts.ID)
                        elts = elts.payload if isinstance(elts.payload, Dot11Elt) else None

                    # Track the number of Vendor Specific IEs in the packet
                    if vendor_specific_count > 0:
                        vendor_specific_counts[vendor_specific_count] += 1

    # Prepare summary table data
    total_probes = len(unique_probes)
    ie_summary = []
    ie_names = {
        1: "Supported rates",
        3: "DS Parameter set",
        45: "HT Capabilities",
        50: "Extended Supported rates",
        70: "RM Enabled Capabilities",
        107: "Interworking",
        114: "Mesh ID",
        127: "Extended Capabilities",
        150: "Vendor Specific (150)",
        191: "VHT Capabilities",
        221: "Vendor Specific (221)"
    }

    # Adding the count for each type of Vendor Specific IEs (2, 3, 4+)
    vendor_specific_summary = {
        "1 Vendor Specific": vendor_specific_counts.get(1, 0),
        "2 Vendor Specific": vendor_specific_counts.get(2, 0),
        "3 Vendor Specific": vendor_specific_counts.get(3, 0),
        "4+ Vendor Specific": sum(count for num, count in vendor_specific_counts.items() if num >= 4)
    }

    for ie_id, name in ie_names.items():
        count = ie_counter.get(ie_id, 0)
        percentage = (count / total_probes) * 100 if total_probes > 0 else 0.0
        ie_summary.append([name, count, f"{percentage:.2f}"])

    # Append Vendor Specific counts to the table
    for key, count in vendor_specific_summary.items():
        ie_summary.append([key, count, f"{(count / total_probes) * 100 if total_probes > 0 else 0.0:.2f}"])

    print("Creating table finished")

    return ie_summary, total_probes

