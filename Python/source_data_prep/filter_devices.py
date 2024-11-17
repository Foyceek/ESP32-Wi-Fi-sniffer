import csv
from scapy.all import rdpcap
from collections import defaultdict
import string

# Function to parse 802.11 Probe Request frames
def parse_probe_request(pcap_file):
    print(f"Loading pcap file: {pcap_file}")
    packets = rdpcap(pcap_file)
    print(f"Total packets loaded: {len(packets)}")
    
    devices = defaultdict(lambda: {"SSIDs": set()})

    for i, pkt in enumerate(packets):
        # Debugging: Track packet processing progress
        if i % 1000 == 0:
            print(f"Processing packet {i}/{len(packets)}")

        if pkt.haslayer('Dot11') and pkt.type == 0 and pkt.subtype == 4:  # 0 = management frame, 4 = Probe Request
            # Extract the device MAC address (TA - Transmitter Address)
            device_mac = pkt.addr2
            if not device_mac:
                continue

            # Extract SSID (Info Element for SSID)
            ssid_elements = pkt.getlayer('Dot11ProbeReq').payload
            ssid = None
            for element in ssid_elements:
                if element.ID == 0:  # ID=0 indicates SSID
                    ssid = element.info.decode('utf-8', errors='ignore')
                    break

            # Validate SSID
            if ssid and all(ch in string.printable for ch in ssid):
                devices[device_mac]["SSIDs"].add(ssid)
    
    # Filter devices with at least 2 SSIDs
    filtered_devices = {
        mac: info
        for mac, info in devices.items()
        if len(info["SSIDs"]) >= 2
    }

    print(f"Processing complete. Found {len(filtered_devices)} devices with at least 2 SSIDs.")
    return filtered_devices

# Function to save device fingerprints to a CSV file
def save_to_csv(devices, output_file):
    print(f"Saving data to {output_file}")
    with open(output_file, mode='w', newline='', encoding='utf-8') as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["Device MAC", "Preferred SSIDs"])
        
        for device_mac, info in devices.items():
            ssids_str = ', '.join(info['SSIDs'])
            writer.writerow([device_mac, ssids_str])
    print(f"Data saved successfully to {output_file}")

# Main function
def main():
    pcap_file = "B07_13-11-24.pcap"  # Replace with your pcap file path
    output_file = "filtered_output.csv"    # Specify the output CSV file
    print(f"Starting analysis for file: {pcap_file}")
    devices = parse_probe_request(pcap_file)
    save_to_csv(devices, output_file)
    print("Analysis completed.")

if __name__ == "__main__":
    main()
