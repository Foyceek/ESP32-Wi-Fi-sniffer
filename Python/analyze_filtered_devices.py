import csv
from itertools import combinations
import matplotlib.pyplot as plt

def calculate_similarity(list1, list2):
    """Calculate the similarity percentage between two lists."""
    set1, set2 = set(list1), set(list2)
    intersection = set1 & set2
    return len(intersection) / max(len(set1), len(set2))

def group_devices_by_ssid_similarity(file_path, thresholds):
    # Read the CSV file and parse SSID lists
    devices = []
    with open(file_path, 'r') as file:
        reader = csv.DictReader(file)
        for row in reader:
            mac = row["Device MAC"]
            ssids = [ssid.strip() for ssid in row["Preferred SSIDs"].split(",")]
            devices.append((mac, ssids))

    results = {}

    for threshold in thresholds:
        # Create groups based on similarity
        groups = []
        ungrouped = set(range(len(devices)))

        for i, j in combinations(range(len(devices)), 2):
            if i in ungrouped and j in ungrouped:
                similarity = calculate_similarity(devices[i][1], devices[j][1])
                if similarity >= threshold:
                    # Merge groups if already grouped, or create a new group
                    found_group = False
                    for group in groups:
                        if i in group or j in group:
                            group.update({i, j})
                            found_group = True
                            break
                    if not found_group:
                        groups.append({i, j})
                    ungrouped -= {i, j}

        # Add remaining ungrouped devices as individual groups
        for index in ungrouped:
            groups.append({index})

        # Convert index-based groups to MAC-based groups with their SSIDs
        filtered_groups = []
        for group in groups:
            mac_addresses = [devices[idx][0] for idx in group]
            if len(mac_addresses) >= 2 and len(set(mac_addresses)) >= 2:
                ssids = list(set(ssid for idx in group for ssid in devices[idx][1]))
                filtered_groups.append({
                    "devices": mac_addresses,
                    "ssids": ssids
                })

        results[threshold] = filtered_groups

    return results

def plot_device_counts(results):
    """Plot the number of devices found at each threshold."""
    thresholds = []
    device_counts = []

    for threshold, groups in results.items():
        thresholds.append(threshold * 100)
        device_counts.append(len(groups))

    plt.figure(figsize=(10, 6))
    plt.plot(thresholds, device_counts, marker='o', color='black', linestyle='-')
    plt.title("Devices Found vs. Similarity Threshold")
    plt.xlabel("Similarity Threshold (%)")
    plt.ylabel("Number of Devices Found")
    plt.grid(True)
    plt.show()

def save_analysis_results(results, file_path):
    """Save the analysis results to a text file."""
    with open(file_path, 'w') as file:
        for threshold, groups in results.items():
            file.write(f"\n### Analysis for Similarity Threshold: {threshold * 100}% ###\n")
            for idx, group in enumerate(groups, 1):
                file.write(f"Device({idx}):\n")
                file.write(f"  MAC Addresses: {', '.join(group['devices'])}\n")
                file.write(f"  Preferred SSIDs: {', '.join(group['ssids'])}\n")

# Example usage
file_path = r"C:\Franta\VUT\5_semestr\probe_analyzer\Python\source_data_prep\devices.csv"  # Path to the input file
similarity_thresholds = [0.75, 0.80, 0.85, 0.90, 0.95]  # Editable similarity thresholds

# Perform the analysis
analysis_results = group_devices_by_ssid_similarity(file_path, similarity_thresholds)

# Save the results to a text file
output_file = r"C:\Franta\VUT\5_semestr\probe_analyzer\Python\output_analysis.txt"
save_analysis_results(analysis_results, output_file)

# Output filtered groups and plot the graph
for threshold, groups in analysis_results.items():
    print(f"\n### Analysis for Similarity Threshold: {threshold * 100}% ###")
    for idx, group in enumerate(groups, 1):
        print(f"Device({idx}):")
        print(f"  MAC Addresses: {', '.join(group['devices'])}")
        print(f"  Preferred SSIDs: {', '.join(group['ssids'])}")

# Plot the graph
plot_device_counts(analysis_results)
