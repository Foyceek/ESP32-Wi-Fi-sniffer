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
        second_half = ":".join(mac_parts[3:])
        hashed_second_half = hash_value(second_half, salt) # Anonymize the second half
        return f"{first_half}:{hashed_second_half[:2]}:{hashed_second_half[2:4]}:{hashed_second_half[4:6]}"

    with open(input_csv, "r", newline="", encoding='utf-8') as infile, open(output_csv, "w", newline="", encoding='utf-8') as outfile:
        reader = csv.DictReader(infile)
        fieldnames = reader.fieldnames
        writer = csv.DictWriter(outfile, fieldnames=fieldnames)
        writer.writeheader()

        for row in reader:
            # Determine column names
            mac_column = "MAC" # if "MAC" in row else "Device MAC"
            ssid_column = "SSID" # if "SSID" in row else "Preferred SSIDs"
            
            # Anonymize the MAC address
            if mac_column in row and row[mac_column]:
                row[mac_column] = anonymize_mac(row[mac_column], salt)

            # Anonymize the SSID if not 'Wildcard'
            if ssid_column in row and row[ssid_column] and row[ssid_column] != "Wildcard":
                hashed_ssid = hash_value(row[ssid_column], salt)
                row[ssid_column] = f"SSID_{hashed_ssid}"

            writer.writerow(row)

    print(f"Anonymized data saved to {output_csv}.")