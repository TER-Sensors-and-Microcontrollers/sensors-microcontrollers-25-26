# serial-demo.py
# Reads incoming can packet from the on-car sender
# 
# 
import time
import os
import random
import serial
import threading

from flask import Flask, render_template, g, jsonify
import sqlite3

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)

app = Flask(__name__)
DATABASE = parent_dir +'/database.db'

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

# [TEST] given open serial port object, indefinitely writes mock CAN data 
# (randomized CAN data frames + fixed set of CAN IDs) to serial port
# does not return anything
# BIG ENDIAN!
def feeder(ser:serial.Serial):
    fids = [2047,1, 36, 37, 38, 39, 300]
    weights = [5, 1, 5, 2, 2, 2, 3]
    start = time.time()
    while True:
        bytes_ = randomize_bytes()
        fid = random.choices(fids, weights=weights)[0]
        
        fid_bytes = fid.to_bytes(1, 'big') if fid > 2048 else fid.to_bytes(2, 'big')
        # print("FID ", fid, "FID LENGTH ",len(fid_bytes))
        # print("DATA LENGTH ", len(bytes_))
        data_to_send = fid_bytes + len(bytes_).to_bytes(1,'big') + bytes_ + b"\n"
        ser.write(data_to_send)
        time.sleep(.005)

# given incoming CAN frame, parse and return appropriate ID,DATA,DLC values.
# if incoming frame is not of valid length, return all -1s
# UNPACKS CAN_ID AS BIG ENDIAN!
def parse_frame(frame:bytes):
    if len(frame) < 3 or len(frame) > 12:
        print("Error parsing current frame!")
        print(f"Frame: {frame}")
        return -1,-1,-1
    can_id = (((frame[0] << 8) | frame[1]) & 0x7FF)
    dlc = frame[2]
    data = frame[3:3+dlc]
    if can_id < 0 or can_id > 2048 or dlc < 0 or dlc > 8:
        print("Error: Malformed CAN frame")
        print(f"Frame: {frame}")
        return -1,-1,-1
    return can_id, dlc, data

# given open serial port object, indefinitely reads icoming CAN frame data 
# from serial port, and stores it in Flask database. does not return anything
# prints debug messages if debug flag set to True
def reader(ser:serial.Serial, debug=True):
    # db = get_db()
    # cursor = db.cursor()
    start = time.time()
    while True:
        if ser.in_waiting > 0:
            frame = ser.readline()
            id,dlc,data = parse_frame(frame)
            if id == -1:
                if debug:
                    print("Skipping frame")
                continue
            
            if debug:
                print(f"RECIEVED: {time.time() - start:.2f}    ID: {id}    DLC: {dlc}    DATA: {data}")
            
            match id:
                # interpret data as appropriate type per case
                case _:
                    pass
                # case 1:
                #     message = [id, "test_data1", int(data), time.time()]
                #     cursor.execute(
                #         "INSERT INTO sensor_readings (sensor_id, name, data, timestamp) VALUES (?, ?, ?, ?)",
                #         message
                #     )
                #     db.commit()
                # case 36:
                #     message = [id, "test_data2", int(data), time.time()]
                #     cursor.execute(
                #         "INSERT INTO sensor_readings (sensor_id, name, data, timestamp) VALUES (?, ?, ?, ?)",
                #         message
                #     )
                #     db.commit()
                # case 37:
                #     message = [id, "test_data3", float(data), time.time()]
                #     cursor.execute(
                #         "INSERT INTO sensor_readings (sensor_id, name, data, timestamp) VALUES (?, ?, ?, ?)",
                #         message
                #     )
                #     db.commit()
                # case 38:
                #     message = [id, "test_data4", int(data), time.time()]
                #     cursor.execute(
                #         "INSERT INTO sensor_readings (sensor_id, name, data, timestamp) VALUES (?, ?, ?, ?)",
                #         message
                #     )
                #     db.commit()
                # case 39:
                #     message = [id, "test_data5", int(data), time.time()]
                #     cursor.execute(
                #         "INSERT INTO sensor_readings (sensor_id, name, data, timestamp) VALUES (?, ?, ?, ?)",
                #         message
                #     )
                #     db.commit() 
        else:
            print("Waiting for serial data...") 
        time.sleep(0.5)
    

if __name__ == "__main__":
    ser = None
    print(DATABASE)
    try:
        # loop:// creates a virtual serial port entirely in memory
        ser = serial.Serial('/dev/serial0', baudrate=115200, timeout=1)
        print("Connected successfully to serial0!")
        # ser = serial.serial_for_url('loop://', baudrate=115200, timeout=1)
        # threading.Thread(target=feeder, args=(ser,), daemon=True).start()
        reader(ser,True)
    except KeyboardInterrupt:
        print("Exiting serial read/write.")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
    finally:
        if ser and ser.is_open:
            print("closing serial connection")
            ser.close()
        print("Serial connection closed")

    