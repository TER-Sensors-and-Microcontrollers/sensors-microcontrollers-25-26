# global variable and import suite for serial reader and client programs to use
#
# Alex Lee, Robi
# 10/25/2024 - Edited 3/1/26
import time
import os
import random
import serial
import struct
import threading

from flask import Flask, render_template, g, jsonify
import sqlite3

import numpy as np

# GLOBAL VARS




# port names
PI_RADIO = '/dev/ttyAMA2'

# required for readings that are packed into the same CAN ID (motor controller)
ID_OFFSET = 1000

# length of sensor data array stored in shm
# Steering + IMU + Reserved wheel speeds [unused as of Spring 2025] + Motor + BMS
SENS_NAMES = ["SteeringWheel", "IMUAccelX", "IMUAccelY", "IMUAccelZ", "ImuGyroX",
               "IMUGyroY", "IMUGyroZ","IMUMagnetX", "IMUMagnetY", "IMUMagnetZ", "WSFrontLeft",
               "WSFrontRight", "WSBackLeft", "WSBackRight", "Motor1", "Motor2", 
               "Motor3", "Motor4", "Motor5", "Motor6", "Motor7", "Motor8", "Motor9", "Motor10", 
               "Motor11", "Motor12", "Motor13", "Motor14", "Motor15", "Motor16", 
               "BMSVolt1", "BMSVolt2", "BMSRes1", "BMSRes2", "BMSOV1", "BMSOV2", "BMSResAvg", "BMSOVAvg"]

SHMEM_NMEM = len(SENS_NAMES)
# datatype of sensor readings to be stored in shm
SHMEM_DTYPE = np.float32
SHMEM_MEMB_SIZE = np.dtype(SHMEM_DTYPE).itemsize
SHMEM_TOTAL_SIZE = SHMEM_NMEM * SHMEM_MEMB_SIZE

MOTOR_START_IDX = SENS_NAMES.index("Motor1")
BMS_START_IDX = SENS_NAMES.index("BMSVolt1")
