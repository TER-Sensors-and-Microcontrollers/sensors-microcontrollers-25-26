# database-feeder.py
# Alex Lee 10/4/2025
# inserts sample data into the database. Assumes database is active
import os
import sqlite3
import time
import random

def insert_test_data():
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))   # directory of THIS file
    DB_PATH = os.path.join(BASE_DIR, "../database.db")       # adjust path as needed

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    readings = [
        (1, "motor temp", random.random() * 100, time.time()),
        (2, "battery", random.random() * 100, time.time()),
        (3, "pressure", random.random() * 100, time.time())
    ]
    cursor.executemany(
        "INSERT INTO sensor_readings (sensor_id, name, data, timestamp) VALUES (?, ?, ?, ?)",
        readings
    )
    conn.commit()
    conn.close()

if __name__ == "__main__":
    print("Writing data to table every 1-3 seconds...")
    while True:
        insert_test_data()
        time.sleep(random.randint(1,3))