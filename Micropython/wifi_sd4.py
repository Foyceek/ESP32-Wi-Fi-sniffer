import os
import network
import ubinascii
import time
from machine import Pin, SoftSPI
from sdcard import SDCard

# Define the SoftSPI pins
spisd = SoftSPI(-1,
                miso=Pin(12),   # MISO: PIN 21 (IO 12)
                mosi=Pin(13),   # MOSI: PIN 19 (IO 13)
                sck=Pin(14))    # SCLK: PIN 23 (IO 14)

led = Pin(21, Pin.OUT)
try:
    # Create and mount the file system
    # Initialize the SD card with CS2 pin
    sd = SDCard(spisd, Pin(15))  # CS2: PIN 24 (IO 15)
    vfs = os.VfsFat(sd)
    os.mount(vfs, '/sd')

    # Print the root directory after mounting
    print('Root directory after mounting: {}'.format(os.listdir()))

    # Change to the SD card directory and list its contents
    os.chdir('/sd')
    print('SD Card contains: {}'.format(os.listdir()))

    # Turn off LED to indicate success
    led.value(1)
except Exception as e:
    print("SD card mount failed:", e)
    # Turn on LED to indicate failure
    led.value(0)

# Function to set ESP32 in STA mode and start scanning for Wi-Fi signals
def wifi_sniffer():
    # Initialize WiFi in station mode
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    # Open a CSV file on the SD card to store probe requests
    with open('/sd/probe_requests.csv', 'w') as f:
        # Write header to the CSV file
        f.write("datetime;dst;randomized;rssi;idx;seq_num;ch_freq;ssid;occupancy\n")

        idx = 0  # Index for probe requests
        while True:
            print("Scanning for nearby networks...")
            # Blink LED to indicate scanning
            led.value(0)
            time.sleep(0.1)
            led.value(1)
            time.sleep(0.1)
            led.value(0)
            time.sleep(0.1)
            led.value(1)

            # Perform a Wi-Fi scan
            networks = wlan.scan()  # List of tuples (ssid, bssid, channel, rssi, authmode, hidden)
            
            # Check if any networks were found
            if networks:
                for net in networks:
                    ssid, bssid, channel, rssi, authmode, hidden = net
                    dst_mac = ubinascii.hexlify(bssid, ':').decode()  # Destination MAC
                    randomized = "1"  # Assuming randomized MAC (1 for yes, 0 for no)
                    seq_num = idx  # Use idx as sequence number
                    ch_freq = channel  # Frequency channel
                    occupancy = "0.0"  # Placeholder for occupancy
                    
                    # Create a timestamp using the time module
                    timestamp = time.localtime()
                    timestamp_str = "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:06}".format(
                        timestamp[0], timestamp[1], timestamp[2],
                        timestamp[3], timestamp[4], timestamp[5],
                        int(time.ticks_us() % 1000000)  # Microsecond precision
                    )

                    # Write the data to the CSV file
                    f.write(f"{timestamp_str};{dst_mac};{randomized};{rssi};{idx};{seq_num};{ch_freq};{ssid};{occupancy}\n")
                    idx += 1  # Increment the index for the next entry

                f.flush()  # Ensure data is written to the file immediately
            else:
                print("No networks found")
            
            # Pause before the next scan
            time.sleep(5)

try:
    print("Starting WiFi sniffer...")
    wifi_sniffer()
except KeyboardInterrupt:
    print("Sniffer stopped")
