import csv
from collections import defaultdict

def is_same_instance(probe1, probe2):
    """
    Implements Algorithm 3: Scan Instance Identification
    Returns True if probe1 and probe2 are from the same instance, False otherwise
    """
    # Check if MAC addresses match
    if probe1['MAC'] != probe2['MAC']:
        return False
    
    # First condition: MAC, HAS_WPS, and UUID-E all match
    if probe1['HAS_WPS'] == probe2['HAS_WPS'] and probe1['UUID-E'] == probe2['UUID-E']:
        return True
    
    # Second condition: MAC and IE match, and SN is within range
    if probe1['IE'] == probe2['IE']:
        try:
            sn1 = int(probe1['SN'])
            sn2 = int(probe2['SN'])
            
            # Strict inequality as per algorithm: probe₁.sn < probe₂.sn < probe₁.sn + 5
            if sn1 < sn2 < sn1 + 5:
                return True
        except (ValueError, TypeError):
            # Handle cases where SN might be invalid or missing
            pass
    
    return False

def extract_instances(input_csv, output_csv):
    """
    Extract unique device instances from probe request data.
    
    Args:
        input_csv (str): Path to input CSV file containing probe requests
        output_csv (str): Path to output CSV file for unique instances
    """
    # Read the probes CSV file with explicit UTF-8 encoding and error handling
    probes = []
    try:
        with open(input_csv, 'r', encoding='utf-8') as file:
            reader = csv.DictReader(file)
            for row in reader:
                probes.append(row)
        print(f"Successfully read {len(probes)} probes from {input_csv}")
    except UnicodeDecodeError:
        # If UTF-8 fails, try with a different encoding
        print(f"UTF-8 encoding failed, trying latin-1 for {input_csv}")
        with open(input_csv, 'r', encoding='latin-1') as file:
            reader = csv.DictReader(file)
            for row in reader:
                probes.append(row)
        print(f"Successfully read {len(probes)} probes from {input_csv} using latin-1 encoding")
    except FileNotFoundError:
        print(f"Error: Input file '{input_csv}' not found.")
        return
    except Exception as e:
        print(f"Error reading input file '{input_csv}': {e}")
        return
    
    if not probes:
        print("No probes found in the input file.")
        return
    
    # Group probes by MAC address for efficiency
    mac_groups = defaultdict(list)
    for probe in probes:
        mac_groups[probe['MAC']].append(probe)
    
    print(f"Grouped probes by MAC address: {len(mac_groups)} unique MAC addresses")
    
    # Find unique instances
    instances = []
    processed = set()  # To keep track of processed probes
    
    for mac, mac_probes in mac_groups.items():
        for i, probe in enumerate(mac_probes):
            # Skip if this probe has already been processed
            probe_id = f"{probe['DATE']},{probe['TIME']},{probe['MAC']}"
            if probe_id in processed:
                continue
            
            # Create a new instance with this probe (excluding SN as per requirement)
            instance = {
                'MAC': probe['MAC'],
                'HAS_WPS': probe['HAS_WPS'],
                'UUID-E': probe['UUID-E'],
                'IE': probe['IE'],
                'SSIDs': [probe['SSID']] if probe['SSID'] and probe['SSID'].strip() else []
            }
            
            # Add all matching probes to this instance
            processed.add(probe_id)
            for j, other_probe in enumerate(mac_probes):
                if i == j:  # Skip the same probe
                    continue
                
                other_id = f"{other_probe['DATE']},{other_probe['TIME']},{other_probe['MAC']}"
                if other_id in processed:
                    continue
                
                if is_same_instance(probe, other_probe):
                    processed.add(other_id)
                    if (other_probe['SSID'] and 
                        other_probe['SSID'].strip() and 
                        other_probe['SSID'] not in instance['SSIDs']):
                        instance['SSIDs'].append(other_probe['SSID'])
            
            instances.append(instance)
    
    # Write to instances CSV with explicit UTF-8 encoding
    try:
        with open(output_csv, 'w', newline='', encoding='utf-8') as file:
            fieldnames = ['MAC', 'HAS_WPS', 'UUID-E', 'IE', 'SSIDs']  # Removed SN from fieldnames
            writer = csv.DictWriter(file, fieldnames=fieldnames)
            writer.writeheader()
            
            for instance in instances:
                # Convert SSIDs list to comma-separated string
                instance['SSIDs'] = ','.join(instance['SSIDs'])
                writer.writerow(instance)
        
        print(f"Successfully processed {len(probes)} probes into {len(instances)} unique instances.")
        print(f"Results written to {output_csv}")
    except Exception as e:
        print(f"Error writing to output file '{output_csv}': {e}")