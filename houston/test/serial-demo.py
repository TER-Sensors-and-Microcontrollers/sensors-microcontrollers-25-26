# serial-demo.py
# Simulates what a standard incoming CAN frame may look like from radio.
# 
# 
import time
import os
import random
import serial
import threading
import numpy as np
import struct

from flask import Flask, render_template, g, jsonify
import sqlite3

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)

app = Flask(__name__)
DATABASE = parent_dir +'/database.db'
start = time.time()
# helper func that returns 8 bytes, randomized (CAN DATA bytestring)
def randomize_bytes():
    return os.urandom(8)

# get flask app DB out-of-context (separate file)
def get_db():
    # required to work with flask app outside of the program
    with app.app_context():
        db = getattr(g, '_database', None)
        if db is None:
            db = g._database = sqlite3.connect(DATABASE)
            db.row_factory = sqlite3.Row # Optional: access rows as dictionary-like objects
        return db

# given open serial port object, indefinitely writes mock CAN data 
# (randomized CAN data frames + fixed set of CAN IDs) to serial port
# does not return anything
def feeder(ser:serial.Serial):
    fids = ["0001", "0036", "0037", "0038", "0039", "1710"]
    weights = [10, 50, 20, 20, 20, 2]
    start = time.time()
    while True:
        bytes_ = randomize_bytes()
        b = ' '.join(f'{byte:02x}' for byte in list(bytes_))
        fid = random.choices(fids, weights=weights)[0]
        data_to_send = f"{time.time() - start:.2f} Frame ID: {fid}, Data: {b}\n"
        ser.write(data_to_send.encode())
        time.sleep(.005)

# given open serial port object, indefinitely reads mock CAN data 
# from serial port, prints it, and stores it in Flask database.
# does not return anything
def reader(ser:serial.Serial):
    db = get_db()
    cursor = db.cursor()
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf8").strip()
            index = line.index("ID")
            iD = line[index + 4: index + 8]
            
            '''


            TODO: change this to floats once we get actual values!!!


            '''
            
            iD = int(iD)
            print(iD)
            index2 = line.index("Data")
            Data = line[index2 + 6: index2 + 28]
            Data = Data[0:10]
            Data = Data.replace(" ","")
            int_value = int(Data, 16)
            print(int_value)
            #print(Data)

            match iD:
                case 1:
                    message = [iD, "test_data1", int_value, "m/s", time.time() - start]
                    cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                        message
                    )
                    db.commit()
                case 36:
                    message = [iD, "test_data2", int_value, "psi", time.time() - start]
                    cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                        message
                    )
                    db.commit()
                case 37:
                    message = [iD, "test_data3", int_value, "s", time.time() - start]
                    cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                        message
                    )
                    db.commit()
                case 38:
                    message = [iD, "test_data4", int_value, "celcius", time.time() - start]
                    cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                        message
                    )
                    db.commit()
                case 39:
                    data = randomize_bytes()
                    motor_speed = struct.unpack('<h', data[2:4])[0]
                    message = [iD, "test_data5", motor_speed, "m/s^2", time.time() - start]
                    cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                        message
                    )
                    db.commit() 
                case 1710:
                    data = randomize_bytes()
                    # print("data:", struct.unpack('<H', data[0:2])[0])
                    post_fault_lo = struct.unpack('<H', data[0:2])[0]
                    post_fault_hi = struct.unpack('<H', data[2:4])[0]
                    run_fault_lo = struct.unpack('<H', data[4:6])[0]
                    run_fault_hi = struct.unpack('<H', data[6:])[0]
                    message = [iD, "errors", post_fault_lo, "bits", time.time() - start]
                    # message = [iD, "errors", 0, "bits", time.time() - start]

                    cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                        message
                    )
                    # message = [iD+1, "errors", post_fault_hi, "bits", time.time() - start]
                    # cursor.execute(
                    #     "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                    #     message
                    # )
                    # message = [iD+2, "errors", run_fault_lo, "bits", time.time() - start]
                    # cursor.execute(
                    #     "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                    #     message
                    # )
                    # message = [iD+3, "errors", run_fault_hi, "bits", time.time() - start]
                    # cursor.execute(
                    #     "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                    #     message
                    # )
                    db.commit()                      
        time.sleep(0.005)
    

if __name__ == "__main__":
    ser = None
    print(DATABASE)
    try:
        # loop:// creates a virtual serial port entirely in memory
        ser = serial.serial_for_url('loop://', baudrate=115200, timeout=1)
        threading.Thread(target=feeder, args=(ser,), daemon=True).start()
        # When moving to the RFD900:
        # ser = serial.Serial('/dev/ttyUSB0', baudrate=115200, timeout=1)
        reader(ser)
    except KeyboardInterrupt:
        print("Exiting serial read/write.")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
        print("Serial connection closed")

    