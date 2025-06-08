import csv
from scapy.all import rdpcap, Dot11, Dot11Elt
import binascii
from datetime import datetime
import time
import os

# Force unbuffered output
os.environ['PYTHONUNBUFFERED'] = '1'

def extract_probe_requests(pcap_file, output_csv):
    # Read the pcap file
    print(f"Reading pcap file: {pcap_file}", flush=True)
    try:
        packets = rdpcap(pcap_file)
        total_packets = len(packets)
        print(f"Total packets to process: {total_packets}", flush=True)
    except Exception as e:
        print(f"Error reading pcap file: {e}", flush=True)
        return

    # List to store all packet data
    all_entries = []

    # Track unique probe requests to avoid duplicate counting
    unique_probes = set()
    
    # Counter for debug output
    counter = 0
    errors = 0
    last_time = time.time()
    progress_interval = 5000  # Show progress every 5000 packets

    # Process each packet
    for packet in packets:
        # Increment counter and show progress every X packets
        counter += 1
        if counter % progress_interval == 0:
            current_time = time.time()
            elapsed = current_time - last_time
            packets_per_second = progress_interval / elapsed if elapsed > 0 else 0
            print(f"Processed {counter}/{total_packets} packets ({(counter/total_packets)*100:.2f}%) - Errors: {errors} - Speed: {packets_per_second:.1f} packets/sec", flush=True)
            last_time = current_time
            
        try:
            if packet.haslayer(Dot11):
                try:
                    dot11 = packet[Dot11]

                    # Check if it's a probe request
                    if dot11.type == 0 and dot11.subtype == 4:
                        # Extract MAC address
                        mac_address = dot11.addr2
                        if not mac_address:
                            continue

                        # Get sequence number
                        try:
                            sequence_number = dot11.SC >> 4  # Extract the 12 most significant bits
                        except (AttributeError, TypeError):
                            sequence_number = -1  # Default value if SC is not available

                        # Extract timestamp to create a unique identifier for the packet
                        try:
                            timestamp = datetime.fromtimestamp(float(packet.time))
                            date_str = timestamp.strftime('%Y-%m-%d')
                            time_str = timestamp.strftime('%H:%M:%S.%f')[:-3]  # Keeping milliseconds, truncating microseconds
                        except (AttributeError, ValueError, TypeError):
                            timestamp = datetime.now()  # Fallback to current time
                            date_str = timestamp.strftime('%Y-%m-%d')
                            time_str = timestamp.strftime('%H:%M:%S.%f')[:-3]
                        
                        unique_id = (mac_address, timestamp)

                        if unique_id in unique_probes:
                            continue
                        unique_probes.add(unique_id)

                        # Extract RSSI (if available in the packet)
                        rssi = packet.dBm_AntSignal if hasattr(packet, 'dBm_AntSignal') else "N/A"

                        # Variables to store extracted information
                        ssid = ""
                        has_wps = False
                        uuid_e = ""
                        information_elements = []
                        
                        # Parse Information Elements
                        if packet.haslayer(Dot11Elt):
                            # Initialize a pointer to the first Dot11Elt layer
                            try:
                                elem = packet[Dot11Elt]
                                
                                # Iterate through all elements
                                while elem:
                                    try:
                                        # ID 0 is SSID
                                        if elem.ID == 0:
                                            try:
                                                if hasattr(elem, 'info'):
                                                    # Handle the SSID properly - both as UTF-8 and as hex if it looks binary
                                                    raw_ssid = elem.info
                                                    if not raw_ssid:
                                                        ssid = "Wildcard"
                                                    else:
                                                        # Try to decode as UTF-8 first
                                                        try:
                                                            decoded_ssid = raw_ssid.decode('utf-8', errors='strict')
                                                            # If the decoded string contains mostly printable characters, use it
                                                            if all(c.isprintable() or c.isspace() for c in decoded_ssid):
                                                                ssid = decoded_ssid
                                                            else:
                                                                # Otherwise, convert to hexadecimal
                                                                ssid = f"{binascii.hexlify(raw_ssid).decode('ascii')}"
                                                        except UnicodeDecodeError:
                                                            # If it can't be decoded as UTF-8, convert to hexadecimal
                                                            ssid = f"{binascii.hexlify(raw_ssid).decode('ascii')}"
                                            except Exception:
                                                ssid = "Wildcard"
                                        
                                        # Add element ID and length to information elements list
                                        if hasattr(elem, 'info'):
                                            info_len = len(elem.info) if elem.info is not None else 0
                                            information_elements.append(f"{elem.ID}:{info_len}")
                                        else:
                                            information_elements.append(f"{elem.ID}:0")
                                        
                                        # ID 221 can contain WPS information (Vendor Specific)
                                        if elem.ID == 221 and hasattr(elem, 'info') and elem.info is not None:
                                            # WPS OUI is typically 00:50:F2:04
                                            if len(elem.info) >= 4 and elem.info[0:3] == b'\x00\x50\xf2' and elem.info[3] == 4:
                                                has_wps = True
                                                
                                                # Parse WPS attributes to find UUID-E
                                                pos = 4
                                                while pos < len(elem.info):
                                                    if pos + 4 <= len(elem.info):
                                                        attr_type = int.from_bytes(elem.info[pos:pos+2], byteorder='big')
                                                        attr_len = int.from_bytes(elem.info[pos+2:pos+4], byteorder='big')
                                                        
                                                        # UUID-E attribute type is 0x1047
                                                        if attr_type == 0x1047 and pos + 4 + attr_len <= len(elem.info):
                                                            uuid_raw = elem.info[pos+4:pos+4+attr_len]
                                                            uuid_e = binascii.hexlify(uuid_raw).decode('utf-8')
                                                        
                                                        pos += 4 + attr_len
                                                    else:
                                                        break
                                        
                                        # Get the next element
                                        elem = elem.payload if elem.payload and isinstance(elem.payload, Dot11Elt) else None
                                    except (AttributeError, Exception) as e:
                                        # Handle malformed packets
                                        if errors <= 10:  # Limit error output to avoid flooding
                                            print(f"Warning at packet {counter}: Error processing Dot11Elt element: {e}", flush=True)
                                        # Try to move to the next element if possible
                                        try:
                                            elem = elem.payload if elem.payload else None
                                        except Exception:
                                            elem = None
                            except Exception as e:
                                if errors <= 10:
                                    print(f"Error accessing Dot11Elt in packet {counter}: {e}", flush=True)
                                errors += 1

                        # Join the information elements list
                        ie_str = ",".join(information_elements)
                        
                        # Append the data to the list with date and time as the first two columns, RSSI as the last
                        all_entries.append([date_str, time_str, mac_address, has_wps, uuid_e, ie_str, sequence_number, ssid, rssi])
                except Exception as e:
                    errors += 1
                    if errors <= 10:  # Limit error output to avoid flooding
                        print(f"Error processing packet {counter}: {e}", flush=True)
                    elif errors == 11:
                        print("Too many errors, suppressing further error messages...", flush=True)
        except Exception as e:
            errors += 1
            if errors <= 10:
                print(f"Error in main packet processing loop at packet {counter}: {e}", flush=True)
            elif errors == 11:
                print("Too many errors, suppressing further error messages...", flush=True)

    # Write the results to a CSV file
    print(f"\nWriting {len(all_entries)} probe requests to {output_csv}", flush=True)
    try:
        with open(output_csv, mode='w', newline='', encoding='utf-8') as csv_file:
            writer = csv.writer(csv_file)

            # Write CSV header with RSSI as the last column
            writer.writerow(["DATE", "TIME", "MAC", "HAS_WPS", "UUID-E", "IE", "SN", "SSID", "RSSI"])

            # Write device data
            for entry in all_entries:
                writer.writerow(entry)
        
        print(f"Extraction complete. Found {len(all_entries)} unique probe requests.", flush=True)
        print(f"Total errors encountered: {errors}", flush=True)
    except Exception as e:
        print(f"Error writing to CSV file: {e}", flush=True)