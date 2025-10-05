# app.py
# 
#  hosts the webapp, initializes the SQLite
#  database, and (tentatively) defines several relevant routes

# Importing flask module in the project is mandatory
# An object of Flask class is our WSGI application.
from flask import Flask, render_template, g
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

# The route() function of the Flask class is a decorator, 
# which tells the application which URL should call 
# the associated function.
@app.route('/')
# ‘/’ URL is bound with run() function. 
def display_index():
    
    send_to_html = 67
    # return render_template("index.html", array_data = shm_data)
    return render_template("index.html", data = send_to_html)

"""
DATABASE INITIALIZATION & ROUTES
"""

# Grab entire db of sensor readings
@app.route('/readings')
def list_users():
    db = get_db()
    cursor = db.cursor()
    cursor.execute("SELECT * FROM sensor_readings")
    readings = cursor.fetchall()
    return str([dict(r) for r in readings]) # Example output

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
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sensor_id INTEGER, " \
        "name TEXT NOT NULL," \
        "data REAL," \
        "timestamp DATETIME" \
        ")")
        db.commit()
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

