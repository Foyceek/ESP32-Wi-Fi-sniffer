import os
import machine
import sdcard

# Define SPI pins for the SD card
spi = machine.SPI(-1, baudrate=115200, polarity=0, phase=0, sck=machine.Pin(14), mosi=machine.Pin(13), miso=machine.Pin(12))
cs = machine.Pin(15, machine.Pin.OUT)

# Initialize the LED on PIN 21
led = machine.Pin(21, machine.Pin.OUT)

# Initialize the SD card
try:
    sd = sdcard.SDCard(spi, cs)
    vfs = os.VfsFat(sd)
    os.mount(vfs, "/sd")
    print("SD card mounted successfully.")
    
    # Turn off LED to indicate success
    led.value(1)
    
    # Check files and directories on the SD card
    print("Files on SD card:")
    files = os.listdir('/sd')
    for file in files:
        print(" -", file)
except Exception as e:
    print("SD card mount failed:", e)
    # Turn on LED to indicate failure
    led.value(0)
