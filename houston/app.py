# app.py
# 
#  hosts the webapp, initializes the SQLite
#  database, and (tentatively) defines several relevant routes

# Importing flask module in the project is mandatory
# An object of Flask class is our WSGI application.
from flask import Flask, render_template, g, jsonify
import sqlite3
from flask_socketio import SocketIO
import threading
from multiprocessing import shared_memory
import time
import atexit
# import serial
#from ER-sensors-microcontrollers.pi.globals import *
import numpy as np

# Flask constructor takes the name of 
# current module (__name__) as argument.
app = Flask(__name__)
socketio = SocketIO(app)
DATABASE = 'database.db'


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

# /get_dp/<sensor_id>
# given id of sensor, returns most recent reading of that id


@app.route('/get_dp/<sensor_id>')
def get_datapoint(sensor_id:int):
    db = get_db()
    cursor = db.cursor()
    # '?' character in the cursor query uses the value passed in from get_datapoint
    # otherwise using sensor_id = sensor_id would query for itself
   
    cursor.execute(
        "SELECT * FROM sensor_readings "
        "WHERE sensor_id = ?"
        "ORDER BY timestamp DESC LIMIT 1",
        (sensor_id,)
    )
    rows = cursor.fetchall()
    cursor.close()

    # return error if the query does not return any results
    if not rows:
        return jsonify({"error": "No up-to-date sensor data found for id " + sensor_id}), 404
    
    # turn reading into JSON object
    reading = {
        "reading_id": rows[0][0],
        "sensor_id": rows[0][1],
        "name": rows[0][2],
        "data": rows[0][3],
        "unit": rows[0][4],
        "timestamp": rows[0][5]
    }

    return jsonify(reading)

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

# /faults
# 
# returns JSON response of all faults found in the Motor Controller
# based on latest database results of ids 1710 -> 1713
@app.route('/faults')
def get_faults():
    errors = []
    db = get_db()
    cursor = db.cursor()
    # Post faults low bits (2 Bytes)
    cursor.execute(
        "SELECT * FROM sensor_readings "
        "WHERE sensor_id = 1710"
        "ORDER BY timestamp DESC LIMIT 1",
    )
    row = cursor.fetchall()
    bits = row[0][3].to_bytes(2, 'little')
    if (bits[0] >> 1 & 1):
        errors.append({"type": "post", "error": "HW de-saturation fault"})
    if (bits[0] >> 2 & 1):
        errors.append({"type": "post", "error": "HW Over-current fault"})
    if (bits[0] >> 3 & 1):
        errors.append({"type": "post", "error": "Accelerator Shorted"})
    if (bits[0] >> 4 & 1):
        errors.append({"type": "post", "error": "Accelerator Open"})
    if (bits[0] >> 5 & 1):
        errors.append({"type": "post", "error": "Current Sensor Low"})
    # if (bits[0] >> 5 & 1):
    #     errors.append({"type": "post", "error": "Current Sensor High"}) 
    #we lowk didn't know how to do the bits for this because the conversion was weird...
    if (bits[0] >> 6 & 1):
        errors.append({"type": "post", "error": "Module Temperature Low"})
    if (bits[0] >> 7 & 1):
        errors.append({"type": "post", "error": "Module Temperature High"})


   
   #for now this is gone....this is what left after the 8 we did for high
    # if (bits[0] >> 17 & 1):
    #     errors.append({"type": "post", "error": "2.5V Sense Voltage Low"})
    # if (bits[0] >> 16 & 1):
    #     errors.append({"type": "post", "error": "12V Sense Voltage High"})
    # if (bits[0] >> 18 & 1):
    #     errors.append({"type": "post", "error": "2.5V Sense Voltage High"})
    # if (bits[0] >> 19 & 1):
    #     errors.append({"type": "post", "error": "1.5V Sense Voltage Low"})
    # if (bits[0] >> 20 & 1):
    #     errors.append({"type": "post", "error": "1.5V Sense Voltage High"})
    # if (bits[0] >> 21 & 1):
    #     errors.append({"type": "post", "error": "DC Bus Voltage High"})
    # if (bits[0] >> 22 & 1):
    #     errors.append({"type": "post", "error": "DC Bus Voltage Low"})
    # if (bits[0] >> 23 & 1):
    #     errors.append({"type": "post", "error": "Pre-charge Timeout"})
    # if (bits[0] >> 24 & 1):
    #     errors.append({"type": "post", "error": "Pre-charge Voltage Failure"})
    # if (bits[0] >> 25 & 1):
    #     errors.append({"type": "post", "error": "EEPROM Checksum Invalid"})
    # if (bits[0] >> 26 & 1):
    #     errors.append({"type": "post", "error": "EEPROM Data Out of Range"})
    # if (bits[0] >> 27 & 1):
    #     errors.append({"type": "post", "error": "EEPROM Update Required"})
    # if (bits[0] >> 28 & 1):
    #     errors.append({"type": "post", "error": "Reserved"})
    # if (bits[0] >> 29 & 1):
    #     errors.append({"type": "post", "error": "Reserved"}) #showed up twice only wrote if statement once
    # if (bits[0] >> 31 & 1):
    #     errors.append({"type": "post", "error": "Brake Shorted"})
    # if (bits[0] >> 32 & 1):
    #     errors.append({"type": "post", "error": "Brake Open"})



    # Post faults high bits (2 Bytes)!!!
    cursor.execute(
        "SELECT * FROM sensor_readings "
        "WHERE sensor_id = 1711"
        "ORDER BY timestamp DESC LIMIT 1",
    )
    row = cursor.fetchall()
    bits = row[0][3].to_bytes(2, 'little')
    if (bits[0] >> 9 & 1):
        errors.append({"type": "post", "error": "Control PCB Temperature Low"})
    if (bits[0] >> 10 & 1):
        errors.append({"type": "post", "error": "Control PCB Temperature High"})
    if (bits[0] >> 11 & 1):
        errors.append({"type": "post", "error": "Gate Drive PCB Temperature Low"})
    if (bits[0] >> 12 & 1):
        errors.append({"type": "post", "error": "Gate Drive PCB Temperature High"})
    if (bits[0] >> 13 & 1):
        errors.append({"type": "post", "error": "5V Sense Voltage Low"})
    if (bits[0] >> 14 & 1):
        errors.append({"type": "post", "error": "5V Sense Voltage High"})
    if (bits[0] >> 15 & 1):
        errors.append({"type": "post", "error": "12V Sense Voltage Low"})
    if (bits[0] >> 16 & 1):
        errors.append({"type": "post", "error": "12V Sense Voltage High"})

    

    # Run faults low bits (2 Bytes)
    cursor.execute(
        "SELECT * FROM sensor_readings "
        "WHERE sensor_id = 1712"
        "ORDER BY timestamp DESC LIMIT 1",
    )
    row = cursor.fetchall()
    bits = row[0][3].to_bytes(2, 'little')
    if (bits[0] >> 1 & 1):
        errors.append({"type": "run", "error": "Motor Over-speed Fault"})
    if (bits[0] >> 2 & 1):
        errors.append({"type": "run", "error": "Over Current Fault"})
    if (bits[0] >> 3 & 1):
        errors.append({"type": "run", "error": "Over Voltage Fault"})
    if (bits[0] >> 4 & 1):
        errors.append({"type": "run", "error": "Inverter Over Temperature Fault"})
    if (bits[0] >> 5 & 1):
        errors.append({"type": "run", "error": "Accelerator Input Shorted Fault"})
    if (bits[0] >> 6 & 1):
        errors.append({"type": "run", "error": "Accelerator Input Open Fault"})
    if (bits[0] >> 7 & 1):
        errors.append({"type": "run", "error": "Direction Command Fault"})
    if (bits[0] >> 8 & 1):
        errors.append({"type": "run", "error": "Inverter Response Time-out Fault"})
    
    #the rest after the first 16...
    # if (bits[0] >> 17 & 1):
    #     errors.append({"type": "run", "error": "Brake Input Shorted Fault"})
    # # if (bits[0] >> 14 & 1):
    # #     errors.append({"type": "run", "error": "Brake Input Open Fault"})
    #!!!!!!! This matches the reserved if statments below (line 342).....


    # if (bits[0] >> 19 & 1):
    #     errors.append({"type": "run", "error": "Module A Over-temperature Fault"})
    # if (bits[0] >> 20 & 1):
    #     errors.append({"type": "run", "error": "Module B Over-temperature Fault"})
    # if (bits[0] >> 21 & 1):
    #     errors.append({"type": "run", "error": "Module C Over-temperature Fault"})
    # if (bits[0] >> 22 & 1):
    #     errors.append({"type": "run", "error": "PCB Over-temperature"})
    # if (bits[0] >> 23 & 1):
    #     errors.append({"type": "run", "error": "GDB1 Over-temperature"})
    # if (bits[0] >> 24 & 1):
    #     errors.append({"type": "run", "error": "GDB 2 Over-temperature"})
    # if (bits[0] >> 25 & 1):
    #     errors.append({"type": "run", "error": "GDB 3 Over-temperature"})
    # if (bits[0] >> 26 & 1):
    #     errors.append({"type": "run", "error": "Current Sensor Fault"})
    # if (bits[0] >> 27 & 1):
    #     errors.append({"type": "run", "error": "Gate Driver Over Voltage"})
    # if (bits[0] >> 28 & 1):
    #     errors.append({"type": "run", "error": "Reserved"})
    # if (bits[0] >> 29 & 1):
    #     errors.append({"type": "run", "error": "HW Over Voltage"})
    # if (bits[0] >> 30 & 1):
    #     errors.append({"type": "run", "error": "Reserved"})
    # if (bits[0] >> 31 & 1):
    #     errors.append({"type": "run", "error": "Resolver Fault"})
    # if (bits[0] >> 32 & 1):
    #     errors.append({"type": "run", "error": "Reserved"})
    
    
    # Run faults high bits (2 Bytes)
    cursor.execute(
        "SELECT * FROM sensor_readings "
        "WHERE sensor_id = 1713"
        "ORDER BY timestamp DESC LIMIT 1",
    )
    row = cursor.fetchall()
    bits = row[0][3].to_bytes(2, 'little')
    if (bits[0] >> 9 & 1):
        errors.append({"type": "run", "error": "HW de-saturation fault"})
    if (bits[0] >> 10 & 1):
        errors.append({"type": "run", "error": "HW Over-current fault"})
    if (bits[0] >> 11 & 1):
        errors.append({"type": "run", "error": "Under-Voltage Fault"})
    if (bits[0] >> 12 & 1):
        errors.append({"type": "run", "error": "CAN Command Message Lost Fault"})
    if (bits[0] >> 13 & 1):
        errors.append({"type": "run", "error": "Motor Over Temperature Fault"})
    if (bits[0] >> 14 & 1):
        errors.append({"type": "run", "error": "Reserved"})
    if (bits[0] >> 15 & 1):
        errors.append({"type": "run", "error": "Reserved"})
    if (bits[0] >> 16 & 1):
        errors.append({"type": "run", "error": "Reserved"})



    cursor.close()

    return jsonify(errors)

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

# main driver function

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
        cursor.close()
    # Runs the app using socketio for real-time data updates from the
    # shared memory.
    socketio.run(app, host="0.0.0.0", port=5000, debug = True, allow_unsafe_werkzeug=True)






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

