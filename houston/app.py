# app.py
# 
#  hosts the webapp, initializes the SQLite
#  database, and (tentatively) defines several relevant routes

# Importing flask module in the project is mandatory
# An object of Flask class is our WSGI application.
from gevent import monkey
monkey.patch_all()

from flask import Flask, render_template, g, jsonify, send_file, request
import sqlite3
from flask_socketio import SocketIO, emit
import threading
from multiprocessing import shared_memory
import time
import atexit
from datetime import datetime
import struct

# import serial
#from ER-sensors-microcontrollers.pi.globals import *
import numpy as np

# Flask constructor takes the name of 
# current module (__name__) as argument.
app = Flask(__name__)
socketio = SocketIO(app)
DATABASE = 'database.db'

start = time.time()
"""
HOME SCREEN ROUTE
"""

# The route() function of the Flask class is a decorator, 
# which tells the application which URL should call 
# the associated function.
@app.route('/')
# ‘/’ URL is bound with run() function. 
def display_index():


    # return render_template("index.html", data=data_to_send)
    return render_template("index.html",)
    #cursor.close

    # return render_template("index.html", array_data = shm_data)
    #return render_template("index.html", data = data_to_send)

@app.route('/SD')
def display_SD():
    return render_template("SD.html",)

@app.route('/max-graph')
def display_graph():
    return render_template("max-graph.html",)
"""
OTHER ROUTES / ENDPOINTS
"""

# /readings
# Grab entire db of sensor readings
@app.route('/readings')
def list_users():
    db = get_db()
    cursor = db.cursor()
    cursor.execute("SELECT * FROM sensor_readings")
    readings = cursor.fetchall()
    cursor.close()
    return str([dict(r) for r in readings]) # Example output

# /get_test/<sensor_id>
# given id of test sensor where:
# 
# 1 = motor_temp
# 2 = battery
# 3 = pressure
# 
# returns JSON response of most recently saved reading of this id.
# JSON payload is as follows:
#  {
#     "reading_id": int,
#     "sensor_id": int,
#     "name": str,
#     "data": float/real,
#     "timestamp": datetime
#  }

@app.route('/get_all_data/<sensor_id>')
def get_all_data_db(sensor_id:int):
    db = get_db()
    cursor = db.cursor()
    cursor.execute("SELECT * FROM sensor_readings " \
    "WHERE sensor_id = ?", (sensor_id,))
    row = cursor.fetchall()
    cursor.close()

    if not row:
        return jsonify({"error": "No sensor data found for id " + sensor_id}), 404

    readings = []
    for r in row:
        reading = {
            "reading_id": r[0],
            "sensor_id": r[1],
            "name": r[2],
            "data": r[3],
            "unit": r[4],
            "timestamp": r[5]
        }
        readings.append(reading)

    return jsonify(readings)

# /unique_sensors
# 
# returns JSON response of all unique sensor id,name,unit tuples ordered by id, ascending
# 404 if no sensors found in db

@app.route('/unique_sensors')
def get_unique_sensors():
    db = get_db()
    cursor = db.cursor()
    cursor.execute("SELECT DISTINCT sensor_id,name,unit FROM sensor_readings ORDER BY sensor_id")
    rows = cursor.fetchall()

    if not rows:
        return jsonify({"error": "No sensor data found"}), 404
    
    cursor.close()

    return [dict(r) for r in rows]

def emit_unique_sensors(app):
    with app.app_context():
        db = get_db()
        while True:
            try:
                # print("emitting unique sens...")
                cursor = db.cursor()
                cursor.execute("SELECT DISTINCT sensor_id,name,unit FROM sensor_readings ORDER BY sensor_id")
                rows = cursor.fetchall()
                unique = [dict(r) for r in rows]
                socketio.emit("unique_sens", unique)
                socketio.sleep(.005)
            except Exception as e:
                print(f"emit_unique_sensors error: {e}")
                socketio.sleep(1)     
            finally:
                cursor.close()     

# emit_faults
# 
# emits dictionary response of all faults found in the Motor Controller
# based on latest database results of ids 1710 -> 1713
def emit_faults(app):
    with app.app_context():
        db = get_db()
        while True:
            try:
                cursor = db.cursor()
                errors = []
                # Post faults low bits (2 Bytes)
                cursor.execute(
                    "SELECT * FROM sensor_readings "
                    "WHERE sensor_id = 1710 "
                    "ORDER BY timestamp DESC LIMIT 1",
                )
                
                # row[0] is first result, row[0][3] is data portion of reading
                row = cursor.fetchall()
                if row:
                    # print("row found!", row[0][3])
                    bits = int(row[0][3]).to_bytes(2, 'little')
                    # print("error bits: ", bits)
                    if (bits[0] >> 0 & 1):
                        errors.append({"type": "post", "error": "HW de-saturation fault"})
                    if (bits[0] >> 1 & 1):
                        errors.append({"type": "post", "error": "HW Over-current fault"})
                    if (bits[0] >> 2 & 1):
                        errors.append({"type": "post", "error": "Accelerator Shorted"})
                    if (bits[0] >> 3 & 1):
                        errors.append({"type": "post", "error": "Accelerator Open"})
                    if (bits[0] >> 4 & 1):
                        errors.append({"type": "post", "error": "Current Sensor Low"})
                    if (bits[0] >> 5 & 1):
                        errors.append({"type": "post", "error": "Current Sensor High"})
                    if (bits[0] >> 6 & 1):
                        errors.append({"type": "post", "error": "Module Temperature Low"})
                    if (bits[0] >> 7 & 1):
                        # print("bit 7 is high")
                        errors.append({"type": "post", "error": "Module Temperature High"})
                    
                    if (bits[1] >> 0 & 1):
                        errors.append({"type": "post", "error": "Control PCB Temperature Low"})
                    if (bits[1] >> 1 & 1):
                        errors.append({"type": "post", "error": "Control PCB Temperature High"})
                    if (bits[1] >> 2 & 1):
                        errors.append({"type": "post", "error": "Gate Drive PCB Temperature Low"})
                    if (bits[1] >> 3 & 1):
                        errors.append({"type": "post", "error": "Gate Drive PCB Temperature High"})
                    if (bits[1] >> 4 & 1):
                        errors.append({"type": "post", "error": "5V Sense Voltage Low"})
                    if (bits[1] >> 5 & 1):
                        errors.append({"type": "post", "error": "5V Sense Voltage High"})
                    if (bits[1] >> 6 & 1):
                        errors.append({"type": "post", "error": "12V Sense Voltage Low"})
                    if (bits[1] >> 7 & 1):
                        errors.append({"type": "post", "error": "12V Sense Voltage High"})
                # Post faults high bits (2 Bytes)
                cursor.execute(
                    "SELECT * FROM sensor_readings "
                    "WHERE sensor_id = 1711 "
                    "ORDER BY timestamp DESC LIMIT 1"
                )
                row = cursor.fetchall()
                if row:
                    bits = int(row[0][3]).to_bytes(2, 'little')

                    if (bits[0] >> 0 & 1):
                        errors.append({"type": "post", "error": "2.5V Sense Voltage Low"})
                    if (bits[0] >> 1 & 1):
                        errors.append({"type": "post", "error": "2.5V Sense Voltage High"})
                    if (bits[0] >> 2 & 1):
                        errors.append({"type": "post", "error": "1.5V Sense Voltage Low"})
                    if (bits[0] >> 3 & 1):
                        errors.append({"type": "post", "error": "1.5V Sense Voltage High"})
                    if (bits[0] >> 4 & 1):
                        errors.append({"type": "post", "error": "DC Bus Voltage High"})
                    if (bits[0] >> 5 & 1):
                        errors.append({"type": "post", "error": "DC Bus Voltage Low"})
                    if (bits[0] >> 6 & 1):
                        errors.append({"type": "post", "error": "Pre-charge Timeout"})
                    if (bits[0] >> 7 & 1):
                        errors.append({"type": "post", "error": "Pre-charge Voltage Failure"})

                    if (bits[1] >> 0 & 1):
                        errors.append({"type": "post", "error": "EEPROM Checksum Invalid"})
                    if (bits[1] >> 1 & 1):
                        errors.append({"type": "post", "error": "EEPROM Data Out of Range"})
                    if (bits[1] >> 2 & 1):
                        errors.append({"type": "post", "error": "EEPROM Update Required"})
                    # bits[1] >> 3, 4, 5 are reserved 
                    if (bits[1] >> 6 & 1):
                        errors.append({"type": "post", "error": "Brake Shorted"})
                    if (bits[1] >> 7 & 1):
                        errors.append({"type": "post", "error": "Brake Open"})

                # Run faults low bits (2 Bytes)
                cursor.execute(
                    "SELECT * FROM sensor_readings "
                    "WHERE sensor_id = 1712 "
                    "ORDER BY timestamp DESC LIMIT 1"
                )
                row = cursor.fetchall()
                if row:
                    bits = int(row[0][3]).to_bytes(2, 'little')

                    if (bits[0] >> 0 & 1):
                        errors.append({"type": "run", "error": "Motor Over-speed Fault"})
                    if (bits[0] >> 1 & 1):
                        errors.append({"type": "run", "error": "Over-current Fault"})
                    if (bits[0] >> 2 & 1):
                        errors.append({"type": "run", "error": "Over-voltage Fault"})
                    if (bits[0] >> 3 & 1):
                        errors.append({"type": "run", "error": "Inverter Over-temperature Fault"})
                    if (bits[0] >> 4 & 1):
                        errors.append({"type": "run", "error": "Accelerator Input Shorted Fault"})
                    if (bits[0] >> 5 & 1):
                        errors.append({"type": "run", "error": "Accelerator Input Open Fault"})
                    if (bits[0] >> 6 & 1):
                        errors.append({"type": "run", "error": "Direction Command Fault"})
                    if (bits[0] >> 7 & 1):
                        errors.append({"type": "run", "error": "Inverter Response Time-out Fault"})

                    if (bits[1] >> 0 & 1):
                        errors.append({"type": "run", "error": "Hardware Gate/Desaturation Fault"})
                    if (bits[1] >> 1 & 1):
                        errors.append({"type": "run", "error": "Hardware Over-current Fault"})
                    if (bits[1] >> 2 & 1):
                        errors.append({"type": "run", "error": "Under-voltage Fault"})
                    if (bits[1] >> 3 & 1):
                        errors.append({"type": "run", "error": "CAN Command Message Lost Fault"})
                    if (bits[1] >> 4 & 1):
                        errors.append({"type": "run", "error": "Motor Over-temperature Fault"})
                # bits[1] >> 5, 6, 7 are reserved

                # Post faults high bits (2 Bytes)
                cursor.execute(
                    "SELECT * FROM sensor_readings "
                    "WHERE sensor_id = 1713 "
                    "ORDER BY timestamp DESC LIMIT 1"
                )
                row = cursor.fetchall()
                if row:
                    bits = int(row[0][3]).to_bytes(2, 'little')

                    if (bits[0] >> 0 & 1):
                        errors.append({"type": "run", "error": "Brake Input Shorted Fault"})
                    if (bits[0] >> 1 & 1):
                        errors.append({"type": "run", "error": "Brake Input Open Fault"})
                    if (bits[0] >> 2 & 1):
                        errors.append({"type": "run", "error": "Module A Over-temperature Fault"})
                    if (bits[0] >> 3 & 1):
                        errors.append({"type": "run", "error": "Module B Over-temperature Fault"})
                    if (bits[0] >> 4 & 1):
                        errors.append({"type": "run", "error": "Module C Over-temperature Fault"})
                    if (bits[0] >> 5 & 1):
                        errors.append({"type": "run", "error": "PCB Over-temperature"})
                    if (bits[0] >> 6 & 1):
                        errors.append({"type": "run", "error": "GDB1 Over-temperature"})
                    if (bits[0] >> 7 & 1):
                        errors.append({"type": "run", "error": "GDB2 Over-temperature"})

                    if (bits[1] >> 0 & 1):
                        errors.append({"type": "run", "error": "GDB3 Over-temperature"})
                    if (bits[1] >> 1 & 1):
                        errors.append({"type": "run", "error": "Current Sensor Fault"})
                    if (bits[1] >> 2 & 1):
                        errors.append({"type": "run", "error": "Gate Driver Over-voltage"})
                    # bits[1] >> 3 reserved
                    if (bits[1] >> 4 & 1):
                        errors.append({"type": "run", "error": "Hardware Over-voltage"})
                    # bits[1] >> 5 reserved
                    if (bits[1] >> 6 & 1):
                        errors.append({"type": "run", "error": "Resolver Fault"})
                # bits[1] >> 7 reserved
                
                cursor.close()
                # print("emitting faults")
                socketio.emit("mc_faults", errors)
                socketio.sleep(2)
            except Exception as e:
                print(f"emit_faults error: {e}")
                socketio.sleep(1)
            finally:
                cursor.close()
"""
DATABASE INITIALIZATION
"""

# Get database object. If it does not exists, create it
def get_db():
    db = getattr(g, '_database', None)
    if db is None:
        db = g._database = sqlite3.connect(DATABASE)
        db.row_factory = sqlite3.Row # Optional: access rows as dictionary-like objects
    return db

@app.teardown_appcontext
def close_connection(exception):
    db = getattr(g, '_database', None)
    if db is not None:
        db.close()


def emit_dp(app):
    last_timestamp = None
    with app.app_context():
        db = get_db()
        while True:
            try:
                cursor = db.cursor()
                cursor.execute("SELECT * FROM sensor_readings ORDER BY timestamp DESC LIMIT 1")
                row = cursor.fetchone()
                cursor.close()

                if row and row['timestamp'] != last_timestamp:
                    last_timestamp = row['timestamp']
                    reading = {
                        "sensor_id": row['sensor_id'],
                        "name": row['name'],
                        "data": row['data'],
                        "unit": row['unit'],
                        "timestamp": row['timestamp'],
                    }
                    print(f"{datetime.now()} - TRANSMITTING ID: {row['sensor_id']}, DATA: {row['data']}")
                    socketio.emit("new_datapoint", reading)
                    socketio.sleep(.005)
                else:
                    socketio.sleep(.05)
            except Exception as e:
                print(f"emit_dp error: {e}")
                socketio.sleep(1)
            finally:
                cursor.close()
            
# download database file
@app.route('/database.db')
def download_db():
    return send_file('database.db', as_attachment=True)

# upload database file
@app.route('/upload', methods=['POST'])
def upload():
    file = request.files['file']

    if not file.filename.endswith('.db'):
        return jsonify({"error": "Invalid file type"}), 400

    db = getattr(g, '_database', None)
    if db is not None:
        db.close()
        g._database = None
    
    file.save(DATABASE)  # DATABASE = 'database.db' already defined at top
    return jsonify({"success": True})

# main driver function
# def test_task(app):
#     with app.app_context():
#         while True:
#             print("Test")
#             socketio.sleep(1)
#############################################################################
# TODO: get latest sensor readings from db, write it into index.html        #
#############################################################################

if __name__ == '__main__':
    with app.app_context():
        db = get_db()
        cursor = db.cursor()

        # drop table containing old data on restart
        cursor.execute("DROP TABLE IF EXISTS sensor_readings")
        
        # initialize database with columns | ID, NAME, DATA, TIMESTAMP |

        cursor.execute("CREATE TABLE IF NOT EXISTS sensor_readings (" \
        "reading_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sensor_id INTEGER, " \
        "name TEXT NOT NULL," \
        "data REAL," \
        "unit TEXT," \
        "timestamp DATETIME" \
        ")")
        db.commit()
        print("database created!")
        cursor.close()
    # Runs the app using socketio for real-time data updates from the
    # shared memory.
    socketio.start_background_task(emit_dp, app)
    socketio.start_background_task(emit_faults, app)
    socketio.start_background_task(emit_unique_sensors, app)

    # socketio.run(app, host="0.0.0.0", port=5000, debug = True, allow_unsafe_werkzeug=True)
    socketio.run(app, host="0.0.0.0", port=5000)





###############################################################################
###############################################################################
###############################################################################

# TODO : use identical logic to instead grab from SQLite database

# Reads the shared memory and creates an array called data that contains the
# first 4 elements of the shared memory array.
# def read_shm():
#     try:
#         existing_shm = shared_memory.SharedMemory(name="mem123")
#         arr = np.ndarray(shape=10, dtype=np.float32, buffer=existing_shm.buf)
#         # Convert array to string for display
#         data = [arr[i].tolist() for i  in range(3)]
#         existing_shm.close()

#         return data
#     except FileNotFoundError:
#         return ["Error: Read from Shared Memory Failed."]

# # Emits the signal 'update_data' to the index.html file to run the javascript
# # code in the file.
# def background_thread():
#     while True:
#         shm_data = read_shm()
#         socketio.emit('update_data', {'array_data': shm_data})
#         time.sleep(0.5)

# # Creates a background thread that continously updates the display with
# # updated data from the shared memory.
# thread = threading.Thread(target = background_thread, daemon = True)
# thread.start()

