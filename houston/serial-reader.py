# serial-demo.py
# Reads incoming can packet from the on-car sender
# 
# 

from globals import *
from datetime import datetime

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)

app = Flask(__name__)
DATABASE = parent_dir + '/houston/database.db'

# FOR DOCKER ONLY
# DATABASE = "/workspace/database.db"

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

# caution: numpy datatypes are not stored as real numbers in db; saved as bytes instead
# <h => little endian, signed short, <H => little endian, unsigned short
def reader(ser:serial.Serial, debug=True):
    db = get_db()
    cursor = db.cursor()
    while True:
        if ser.in_waiting > 0:
            try:
                frame = ser.readline()
                id,dlc,data = parse_frame(frame)
                if id == -1:
                    if debug:
                        print("Skipping frame")
                    continue
                
                if debug:
                    print(f"RECIEVED: {datetime.now()}    ID: {id}    DLC: {dlc}    DATA: {data}")
                if len(data) != 8: # skip chopped messages [tentatively]
                    print(f"Error: data length is not 8!")
                    continue
                match id:
                    # interpret data as appropriate type per case
                        # START MC Values - Handle as Little Endian
                        case 160: # 'A0' - template value
                            pass
                        case 161: # Temp 2
                            cb_temp = struct.unpack('<h', data[0:2])[0]
                            reading = [id * CONST_OFFSET, "CB Temp", cb_temp * .1,"C", time.time() - start]
                            cursor.execute(
                            "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                                reading
                            )
                            db.commit()
                        case 162: # Temp 3
                            cool_temp = struct.unpack('<h', data[0:2])[0]
                            htspt_temp = struct.unpack('<h', data[2:4])[0]
                            mot_temp = struct.unpack('<h', data[4:6])[0]
                            readingCool = [id * CONST_OFFSET, "Coolant Temp", cool_temp * .1,"C", time.time() - start]
                            readingHtspt = [id * CONST_OFFSET + 1, "Hotspot Temp", htspt_temp * .1,"C", time.time() - start]
                            readingMot = [id * CONST_OFFSET + 2, "Motor Temp", mot_temp * .1,"C", time.time()- start]
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
                            readingA = [id * CONST_OFFSET, "Motor Angle", motor_angle * .1,"Degrees", time.time() - start]
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
                            readingMax = [id * CONST_OFFSET, "Max Cell Voltage", max_cell_volt * .1, "Volts", time.time() - start]
                            readingMin = [id * CONST_OFFSET + 1, "Min Cell Voltage", min_cell_volt * .1, "Volts", time.time() - start]
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
            except Exception as e:
                print(f"Error processing frame: {e}, frame: {frame}")
        else:
            print("Waiting for serial data...") 
            time.sleep(0.5)
        time.sleep(.002)

if __name__ == "__main__":
    ser = None
    print(DATABASE)
    print("Waiting briefly for app startup...")
    time.sleep(3)
    try:
        # loop:// creates a virtual serial port entirely in memory
        ser = serial.Serial(PI_RADIO, baudrate=460800, timeout=1)
        # print(f"Connected successfully to {PI_RADIO}")
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

    