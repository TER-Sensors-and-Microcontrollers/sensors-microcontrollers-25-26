# serial-feeder.py
# Simulates what a standard incoming CAN frame may look like from radio
# 

import time
import os
import random
import serial

def randomize_bytes():
    return os.urandom(8)

try:
    ser = serial.Serial(
        port='/dev/ttyUSB0', # change USB port name if necessary
        baudrate=115200, # standard baud rate for raspberry pi
        timeout=1
    )

    start = time.time()
    # configure number of frame ids / weights if needed
    fids = ["0001", "0036", "00A2", "00A3", "00A4"]
    weights = [1, 5, 2, 2, 2]

    while True:
        bytes = randomize_bytes()
        b = ' '.join(f'{byte:02x}' for byte in list(bytes))
        fid = random.choices(fids, weights=weights)[0]

        data_to_send =  f"{time.time() - start} Frame ID: {fid}, Data: {b}"
        ser.write(data_to_send.encode())
        time.sleep(0.05) 
        
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    exit()

finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Closed serial connection!")

    