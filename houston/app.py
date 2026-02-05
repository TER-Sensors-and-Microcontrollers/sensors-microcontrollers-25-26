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

@app.route('/get_test/<sensor_id>')
def get_test_data(sensor_id:int):
    db = get_db()
    cursor = db.cursor()
    # '?' character in the cursor query uses the value passed in from get_test_data
    # otherwise using sensor_id = sensor_id would query for itself
    # I use sensor_id variable as an argument twice, so I need to pass it in as a parameter twice
    # (the number of ?s) in the query
    cursor.execute("SELECT * FROM sensor_readings " \
    "WHERE sensor_id = ? AND " \
    "timestamp = (SELECT MAX(timestamp) FROM sensor_readings WHERE sensor_id = ?)", (sensor_id, sensor_id))
    rows = cursor.fetchall()
    cursor.close()

    # return error 404 if the query does not return any results
    if not rows:
        return jsonify({"error": "No sensor data found for id " + sensor_id}), 404
    
    # turn reading into JSON object
    reading = {
        "reading_id": rows[0][0],
        "sensor_id": rows[0][1],
        "name": rows[0][2],
        "data": rows[0][3],
        "unit": rows[0][4],
        "timestamp": rows[0][5]
    }
    # reading = Data_Point(rows[0][0], rows[0][1], rows[0][2], rows[0][3], rows[0][4])
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

