import csv
from datetime import datetime

def rewrite_csv(input_file, output_file):
    with open(input_file, 'r') as infile, open(output_file, 'w', newline='') as outfile:
        reader = csv.reader(infile)
        writer = csv.writer(outfile)

        # Write the header
        writer.writerow(['DATE', 'TIME', 'MAC', 'SSID', 'RSSI'])

        for row in reader:
            # Extract the date, time, MAC address, and RSSI from the input row
            timestamp_str, mac, rssi = row[0], row[1], row[2]

            # Convert the timestamp to the desired format (YYYY-MM-DD, HH:MM:SS)
            timestamp = datetime.strptime(timestamp_str, "%a %b %d %H:%M:%S %Y")
            date = timestamp.strftime('%Y-%m-%d')
            time = timestamp.strftime('%H:%M:%S')

            # Write the formatted row to the output file
            writer.writerow([date, time, mac.lower(), '', rssi])