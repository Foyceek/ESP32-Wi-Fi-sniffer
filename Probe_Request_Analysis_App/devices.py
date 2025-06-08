import csv
from collections import defaultdict

def calculate_ssid_similarity(ssids1, ssids2):
    """
    Calculate similarity ratio between two SSID sets, excluding "Wildcard"
    Formula: p = |set(instance₁.SSIDs) ∩ set(instance₂.SSIDs)| / |set(instance₁.SSIDs) ∪ set(instance₂.SSIDs)|
    """
    # Convert comma-separated strings to sets, excluding "Wildcard"
    set1 = set(ssid for ssid in ssids1.split(',') if ssid and ssid != "Wildcard")
    set2 = set(ssid for ssid in ssids2.split(',') if ssid and ssid != "Wildcard")
    
    # If either set is empty after removing "Wildcard", can't calculate meaningful similarity
    if not set1 or not set2:
        return 0.0
    
    # Calculate intersection and union
    intersection = set1.intersection(set2)
    union = set1.union(set2)
    
    # Calculate similarity ratio
    return len(intersection) / len(union) if union else 0.0

def is_same_device(instance1, instance2, threshold):
    """
    Implements Algorithm 4: Device Identification
    Returns True if instance1 and instance2 are from the same device, False otherwise
    """
    # Check MAC address
    if instance1['MAC'] == instance2['MAC']:
        return True
    
    # Check HAS_WPS and UUID-E
    if instance1['HAS_WPS'] == 'True' and instance2['HAS_WPS'] == 'True':
        if instance1['UUID-E'] == instance2['UUID-E']:
            return True
        else:
            return False
    
    # Check IE and SSID similarity
    if instance1['IE'] == instance2['IE']:
        p = calculate_ssid_similarity(instance1['SSIDs'], instance2['SSIDs'])
        if p > threshold:
            return True
        else:
            return False
    
    # Default case if none of the above match
    return False

def extract_devices(input_csv, output_csv, threshold=0.5):
    """
    Extract unique devices from instance data.
    
    Args:
        input_csv (str): Path to input CSV file containing instances (output from extract_instances)
        output_csv (str): Path to output CSV file for unique devices
        threshold (float): SSID similarity threshold for device identification (default: 0.5)
    """
    # Read instances from input CSV file with explicit UTF-8 encoding and error handling
    instances = []
    try:
        with open(input_csv, 'r', encoding='utf-8') as file:
            reader = csv.DictReader(file)
            for row in reader:
                # Only include instances with at least two non-Wildcard SSIDs
                ssids = [s for s in row['SSIDs'].split(',') if s and s != "Wildcard"]
                if len(ssids) >= 2:
                    instances.append(row)
        print(f"Successfully read {len(instances)} instances with at least 2 non-Wildcard SSIDs from {input_csv}")
    except UnicodeDecodeError:
        # If UTF-8 fails, try with a different encoding
        print(f"UTF-8 encoding failed, trying latin-1 for {input_csv}")
        with open(input_csv, 'r', encoding='latin-1') as file:
            reader = csv.DictReader(file)
            for row in reader:
                ssids = [s for s in row['SSIDs'].split(',') if s and s != "Wildcard"]
                if len(ssids) >= 2:
                    instances.append(row)
        print(f"Successfully read {len(instances)} instances with at least 2 non-Wildcard SSIDs from {input_csv} using latin-1 encoding")
    except FileNotFoundError:
        print(f"Error: Input file '{input_csv}' not found.")
        return
    except Exception as e:
        print(f"Error reading input file '{input_csv}': {e}")
        return
    
    if not instances:
        print("No instances with at least 2 non-Wildcard SSIDs found in the input file.")
        return
    
    # Group instances into devices using a disjoint-set (union-find) approach for better clustering
    # First, create a mapping from instance index to device index
    instance_to_device = {}
    devices = []
    
    # Start with each instance in its own device group
    for i, instance in enumerate(instances):
        device = {
            'MACs': [instance['MAC']],
            'HAS_WPS': instance['HAS_WPS'],
            'UUID-E': instance['UUID-E'],
            'IE': instance['IE'],
            'SSIDs': set(s for s in instance['SSIDs'].split(',') if s),
            'instances': [i]
        }
        devices.append(device)
        instance_to_device[i] = len(devices) - 1
    
    # Now merge devices based on the same_device criteria
    for i in range(len(instances)):
        for j in range(i + 1, len(instances)):
            # Skip if already in the same device group
            if instance_to_device[i] == instance_to_device[j]:
                continue
                
            # Check if instances should be in the same device
            if is_same_device(instances[i], instances[j], threshold):
                # Get current device indices
                device_i = instance_to_device[i]
                device_j = instance_to_device[j]
                
                # Merge device_j into device_i
                devices[device_i]['MACs'].extend([mac for mac in devices[device_j]['MACs'] 
                                             if mac not in devices[device_i]['MACs']])
                devices[device_i]['SSIDs'].update(devices[device_j]['SSIDs'])
                devices[device_i]['instances'].extend(devices[device_j]['instances'])
                
                # Update all instances from device_j to point to device_i
                for instance_idx in devices[device_j]['instances']:
                    instance_to_device[instance_idx] = device_i
                
                # Mark device_j as merged by setting to None
                devices[device_j] = None
    
    # Filter out None entries (merged devices) and format the final devices
    final_devices = []
    for device in devices:
        if device is not None:
            final_device = {
                'MACs': ','.join(device['MACs']),
                'HAS_WPS': instances[device['instances'][0]]['HAS_WPS'],
                'UUID-E': instances[device['instances'][0]]['UUID-E'],
                'IE': instances[device['instances'][0]]['IE'],
                'SSIDs': ','.join(device['SSIDs']),
                'instance_count': len(device['instances'])
            }
            final_devices.append(final_device)
    
    # Write devices to output CSV with explicit UTF-8 encoding
    try:
        with open(output_csv, 'w', newline='', encoding='utf-8') as file:
            fieldnames = ['MACs', 'HAS_WPS', 'UUID-E', 'IE', 'SSIDs', 'instance_count']
            writer = csv.DictWriter(file, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(final_devices)
        
        print(f"Successfully identified {len(final_devices)} unique devices from {len(instances)} instances.")
        print(f"Results written to {output_csv}")
    except Exception as e:
        print(f"Error writing to output file '{output_csv}': {e}")
        return
    
    # Print some statistics
    ssid_counts = [len([s for s in device['SSIDs'].split(',') if s and s != "Wildcard"]) for device in final_devices]
    mac_counts = [len(device['MACs'].split(',')) for device in final_devices]
    
    if ssid_counts:
        avg_ssids = sum(ssid_counts) / len(ssid_counts)
        max_ssids = max(ssid_counts)
        print(f"Average non-Wildcard SSIDs per device: {avg_ssids:.2f}")
        print(f"Maximum non-Wildcard SSIDs for a device: {max_ssids}")
    
    if mac_counts:
        devices_with_multiple_macs = sum(1 for count in mac_counts if count > 1)
        max_macs = max(mac_counts)
        print(f"Devices with multiple MAC addresses: {devices_with_multiple_macs} ({devices_with_multiple_macs/len(mac_counts)*100:.2f}%)")
        print(f"Maximum MAC addresses for a device: {max_macs}")