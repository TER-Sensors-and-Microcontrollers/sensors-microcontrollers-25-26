# serial-demo.py
# Reads incoming can packet from the on-car sender
# 
# 

from globals import *

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
    db = get_db()
    cursor = db.cursor()

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
            if len(data) == 8: # skip chopped messages [tentatively]
                print(f"Error: data length is not 8!")
                continue
                
            match id:
                # interpret data as appropriate type per case
                    case 160: # 'A0' - template value
                        pass
                    case 161: # Temp 2
                        cb_temp = np.float32(struct.unpack('<H', data[0:2])[0]) / 10
                        reading = [id, "CB Temp", float(cb_temp), time.time()]
                        cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            reading
                        )
                        db.commit()
                    case 162: # Temp 3
                        cool_temp = np.float32(struct.unpack('<H', data[0:2])[0]) / 10
                        htspt_temp = np.float32(struct.unpack('<H', data[2:4])[0]) / 10
                        mot_temp = np.float32(struct.unpack('<H', data[4:6])[0]) / 10
                        readingCool = [id, "Coolant Temp", float(cool_temp),"todo", time.time()]
                        readingHtspt = [id, "Hotspot Temp", float(htspt_temp),"todo", time.time()]
                        readingMot = [id, "Motor Temp", float(mot_temp),"todo", time.time()]
                        cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            readingCool
                        )
                        cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            readingHtspt
                        )
                        cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            readingMot
                        )
                        db.commit()
                    case 163: # Analog Input
                        bit_string = ''.join(format(byte, '08b') for byte in data) # for bitops.
                        pedal1 = bit_string[-10:]
                        # this grabs the proper bit sequence for the brake pedal
                        pedal2 = bit_string[20:30]
                        readingP1 = [id, "Pedal1", float(pedal1),"todo", time.time()]
                        readingP2 = [id, "Pedal2", float(pedal2),"todo", time.time()]
                        cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            readingP1
                        )
                        cursor.execute(
                        "INSERT INTO sensor_readings (sensor_id, name, data, unit, timestamp) VALUES (?, ?, ?, ?, ?)",
                            readingP2
                        )
                        db.commit()
                    case 164: # Dig. Input Status
                        pass
                    case _:
                        pass
        else:
            print("Waiting for serial data...") 
        time.sleep(0.5)
    

if __name__ == "__main__":
    ser = None
    print(DATABASE)
    try:
        # loop:// creates a virtual serial port entirely in memory
        ser = serial.Serial(PI_RADIO, baudrate=115200, timeout=1)
        print(f"Connected successfully to {PI_RADIO}")
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

    