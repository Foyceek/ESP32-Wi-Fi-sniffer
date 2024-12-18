# import csv
# import hashlib
# import os

# # Generate a random salt once for the entire script
# salt = os.urandom(16).hex()  # Random 16-byte salt, converted to a hex string

# def hash_value(value, salt):
#     """Hash a value with a salt using SHA256."""
#     return hashlib.sha256(f"{salt}{value}".encode()).hexdigest()[:8]  # Shorten the hash for readability

# def anonymize_mac(mac, salt):
#     """Anonymize the second half of a MAC address, keeping the first half intact."""
#     mac_parts = mac.split(":")
#     # First three bytes remain unchanged, hash the last three bytes
#     first_half = ":".join(mac_parts[:3])
#     second_half = ":".join(mac_parts[3:])
#     hashed_second_half = hash_value(second_half, salt)
#     # Combine the first half with the hashed second half
#     return f"{first_half}:{hashed_second_half[:2]}:{hashed_second_half[2:4]}:{hashed_second_half[4:6]}"

# input_file = r"C:\Franta\VUT\5_semestr\SEP\analize_output\relevant_data.csv"
# output_file = r"C:\Franta\VUT\5_semestr\SEP\analize_output\anonymized_data.csv"

# # Read and anonymize the CSV file
# with open(input_file, "r", newline="") as infile, open(output_file, "w", newline="") as outfile:
#     reader = csv.DictReader(infile)
#     fieldnames = reader.fieldnames
#     writer = csv.DictWriter(outfile, fieldnames=fieldnames)
#     writer.writeheader()

#     for row in reader:
#         # Anonymize the MAC address (partial anonymization)
#         row["Device MAC"] = anonymize_mac(row["Device MAC"], salt)
        
#         # Anonymize the SSID, skipping "Wildcard"
#         if row["Preferred SSIDs"] != "Wildcard":
#             hashed_ssid = hash_value(row["Preferred SSIDs"], salt)
#             row["Preferred SSIDs"] = f"SSID_{hashed_ssid}"
        
#         writer.writerow(row)

# print(f"Anonymized data saved to {output_file}.")

import csv
import hashlib
import os

def anonymize_csv(input_csv, output_csv):
    """
    Anonymize MAC addresses and SSIDs in a CSV file.
    - Keep the first half of the MAC address intact and anonymize the second half.
    - Prefix hashed SSIDs with 'SSID_', skipping SSIDs with the value 'Wildcard'.
    """
    # Generate a random salt once for consistency
    salt = os.urandom(16).hex()

    def hash_value(value, salt):
        """Hash a value with a salt using SHA256."""
        return hashlib.sha256(f"{salt}{value}".encode()).hexdigest()[:8]  # Shortened hash

    def anonymize_mac(mac, salt):
        """Anonymize the second half of a MAC address."""
        mac_parts = mac.split(":")
        first_half = ":".join(mac_parts[:3])  # Keep the first half intact
        second_half = ":".join(mac_parts[3:])  # Anonymize the second half
        hashed_second_half = hash_value(second_half, salt)
        return f"{first_half}:{hashed_second_half[:2]}:{hashed_second_half[2:4]}:{hashed_second_half[4:6]}"

    with open(input_csv, "r", newline="") as infile, open(output_csv, "w", newline="") as outfile:
        reader = csv.DictReader(infile)
        fieldnames = reader.fieldnames
        writer = csv.DictWriter(outfile, fieldnames=fieldnames)
        writer.writeheader()

        for row in reader:
            # Anonymize the MAC address
            row["Device MAC"] = anonymize_mac(row["Device MAC"], salt)

            # Anonymize the SSID if not 'Wildcard'
            if row["Preferred SSIDs"] != "Wildcard":
                hashed_ssid = hash_value(row["Preferred SSIDs"], salt)
                row["Preferred SSIDs"] = f"SSID_{hashed_ssid}"

            writer.writerow(row)

    print(f"Anonymized data saved to {output_csv}.")
