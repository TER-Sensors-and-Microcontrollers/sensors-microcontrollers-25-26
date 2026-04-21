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

CONST_OFFSET = 10
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
    fids = ["0160", "0161", "0162", "0163", "0164", "0165", "0166", "0167", "0168", "0169", "0170", "0171", "0172", "0192", "0193", "0194"]
    weights = [10, 50, 20, 20, 20, 10, 20, 25, 10, 50, 20, 5, 20, 90, 20, 5]
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
    try:
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf8").strip()
                index = line.index("ID")
                iD = line[index + 4: index + 8]
                
                '''


                TODO: change this to floats once we get actual values!!!


                '''
                
                id = int(iD)
                print(iD)
                index2 = line.index("Data")
                Data = line[index2 + 6: index2 + 28]
                Data = Data[0:10]
                Data = Data.replace(" ","")
                int_value = int(Data, 16)
                print(int_value)
                #print(Data)
                data = randomize_bytes()
                match id:
                    # interpret data as appropriate type per case
                        # START MC Values - Handle as Little Endian
                        case 160: # 'A0' - template value
                            pass
                        case 161: # Temp 2
                            cb_temp = struct.unpack('<h', data[0:2])[0]
                            reading = [id * CONST_OFFSET, "CB Temp", round(cb_temp * .1,3),"C", time.time() - start]
                            cursor.execute(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                reading
                            )
                            db.commit()
                        case 162: # Temp 3
                            cool_temp = struct.unpack('<h', data[0:2])[0]
                            htspt_temp = struct.unpack('<h', data[2:4])[0]
                            mot_temp = struct.unpack('<h', data[4:6])[0]
                            readingCool = [id * CONST_OFFSET, "Coolant Temp",round(cool_temp * .1,3),"C", time.time() - start]
                            readingHtspt = [id * CONST_OFFSET + 1, "Hotspot Temp", round(htspt_temp * .1, 3),"C", time.time() - start]
                            readingMot = [id * CONST_OFFSET + 2, "Motor Temp", round(mot_temp * .1, 3),"C", time.time()- start]
                            cursor.executemany(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                [readingCool, readingHtspt, readingMot]
                            )
                            db.commit()
                        case 163: # Analog Input - Not impl. this year
                            pass
                        case 164: # Dig. Input Status
                            pass
                        case 165: # Motor Pos.
                            motor_angle = struct.unpack('<h', data[0:2])[0]
                            motor_speed = struct.unpack('<h', data[2:4])[0]
                            readingA = [id * CONST_OFFSET, "Motor Angle", round(motor_angle * .1, 3),"Degrees", time.time() - start]
                            readingS = [id * CONST_OFFSET + 1, "Motor Speed", motor_speed,"RPM", time.time() - start]
                            cursor.executemany(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                [readingA, readingS]
                            )
                            db.commit()
                        case 166: # Current Info
                            dc_curr = struct.unpack('<h', data[6:])[0]
                            reading = [id * CONST_OFFSET, "DC Current", dc_curr * .1,"Amps", time.time()- start]
                            cursor.execute(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                reading
                            )
                            db.commit()
                        case 167: # Voltage Info
                            dc_volt = struct.unpack('<h', data[0:2])[0]
                            reading = [id * CONST_OFFSET, "DC Voltage", dc_volt * .1,"Volts", time.time() - start]
                            cursor.execute(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                reading
                            )
                            db.commit()
                        case 170: # Internal States
                            vsm_state = data[0]
                            inv_state = data[2]
                            other = data[7]
                            readingV = [id * CONST_OFFSET, "VSM State", vsm_state,"Bits", time.time() - start]
                            readingI = [id * CONST_OFFSET + 1, "Inverter State", inv_state,"Bits", time.time() - start]
                            readingO = [id * CONST_OFFSET + 2, "DC Voltage", other,"Bits", time.time() - start]
                            cursor.executemany(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                [readingV, readingI, readingO]
                            )
                            db.commit()
                        case 171: # Fault codes
                            post_fault_lo = struct.unpack('<H', data[0:2])[0]
                            post_fault_hi = struct.unpack('<H', data[2:4])[0]
                            run_fault_lo = struct.unpack('<H', data[4:6])[0]
                            run_fault_hi = struct.unpack('<H', data[6:])[0]
                            readingPl= [id * CONST_OFFSET, "Low Post Faults", post_fault_lo,"Bits", time.time() - start]
                            readingPh = [id * CONST_OFFSET + 1, "High Post Faults", post_fault_hi,"Bits", time.time() - start]
                            readingRl= [id * CONST_OFFSET + 2, "Low Run Faults", run_fault_lo,"Bits", time.time() - start]
                            readingRh = [id * CONST_OFFSET + 3, "High Run Faults", run_fault_hi,"Bits", time.time() - start]
                            cursor.executemany(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                [readingPl, readingPh, readingRl, readingRh]
                            )
                            db.commit()
                        case 172: # Torque / Timer
                            torque = struct.unpack('<h', data[0:2])[0]
                            timer = struct.unpack('<H', data[2:4])[0]
                            readingTq = [id * CONST_OFFSET, "Torque", torque * .1,"N.m.", time.time() - start]
                            readingTm = [id * CONST_OFFSET + 1, "Inverter State", timer * .003,"Seconds", time.time() - start]
                            cursor.executemany(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                [readingTq, readingTm]
                            )
                            db.commit()
                        case 176: # High speed Motor Speed, DC Bus Volt, use if other values are obtained too slowly
                            pass
                            # motor_speed = struct.unpack('<h', data[4:6])[0]
                            # dc_volt = struct.unpack('<h', data[6:8])[0]
                            # readingS = [id * CONST_OFFSET, "Motor Speed", motor_speed,"RPM", time.time() - start]
                            # readingV = [id * CONST_OFFSET + 1, "DC Bus Voltage", dc_volt ,"Volts", time.time() - start]
                            # cursor.execute(
                            # "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            #     readingS
                            # )
                            # cursor.execute(
                            # "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            #     readingV
                            # )
                            # db.commit()
                        # END MC values
                        # START BMS values - Custom messages configured on OrionBMS2 Utility
                        case 192: #0xC0 : Pack data
                            pack_curr = struct.unpack('<H', data[0:2])[0]
                            pack_inst_volt = struct.unpack('<H', data[2:4])[0]
                            pack_resistance = struct.unpack('<H', data[4:6])[0]
                            SOC = data[6]
                            readingCurr = [id * CONST_OFFSET, "Pack Current", pack_curr * .1, "Amps", time.time() - start]
                            readingVolt = [id * CONST_OFFSET + 1, "Pack Instant Voltage", pack_inst_volt * .1, "Volts", time.time() - start]
                            readingRes = [id * CONST_OFFSET + 2, "Pack Resistance", pack_resistance * .01, "Ohm", time.time() - start]
                            readingSOC = [id * CONST_OFFSET + 3, "State of Charge", SOC * .5, "%", time.time() - start]
                            cursor.executemany(
                                "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)", 
                                [readingCurr, readingVolt, readingRes, readingSOC]
                            )
                            db.commit()
                        case 193: # Min/Max Cell Voltage, Min/Max Temp
                            max_cell_volt = struct.unpack('<H', data[0:2])[0]
                            min_cell_volt = struct.unpack('<H', data[2:4])[0]
                            temp_hi = data[4] - 256 if data[4] >= 128 else data[4]
                            temp_lo = data[5] - 256 if data[5] >= 128 else data[5]
                            readingMax = [id * CONST_OFFSET, "Max Cell Voltage", max_cell_volt * 0.0001, "Volts", time.time() - start]
                            readingMin = [id * CONST_OFFSET + 1, "Min Cell Voltage", min_cell_volt * 0.0001, "Volts", time.time() - start]
                            readingH = [id * CONST_OFFSET + 2, "High Temperature", temp_hi, "C", time.time() - start]
                            readingL = [id * CONST_OFFSET + 3, "Low Temperature", temp_lo, "C", time.time() - start]
                            cursor.executemany(
                                "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)", 
                                [readingMax,readingMin,readingH,readingL]
                            )
                            db.commit()
                        case 194: # BMS Fail Codes
                            fault_lo = struct.unpack('<H', data[0:2])[0]
                            fault_hi = struct.unpack('<H', data[2:4])[0]
                            answer_to_everything = data[5]  
                            readingl= [id * CONST_OFFSET, "Low BMS Faults", fault_lo,"Bits", time.time() - start]
                            readingh= [id * CONST_OFFSET + 1, "High BMS Faults", fault_hi,"Bits", time.time() - start]
                            message = [id * CONST_OFFSET + 2, "The Answer to Everything", answer_to_everything, "Apples", time.time() - start]
                            cursor.executemany(
                                "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                [readingl, readingh, message]
                            )
                            db.commit()
                        # END BMS values
                        case _:
                            pass   
            time.sleep(0.005) 
    except Exception as e:
        print(f"error: {e}")
    finally:
        cursor.close()                            
        
    

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

    