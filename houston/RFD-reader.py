# RFD-reader.py
# 
# reads data transmitted from RFD900 to serial port __
# 

import serial
import time

# TODO: replace /dev/serial0 with appropriate port
port = serial.Serial('/dev/serial0', 115200, timeout=1) 

try:
    while True:
        if port.in_waiting > 0:
            data = port.readline().decode('utf-8').strip() # Read a line and decode
            print(f"{data}")
            # do something with the data ...
        time.sleep(0.1)
except KeyboardInterrupt:
    print("Exiting serial read.")
finally:
    port.close()
