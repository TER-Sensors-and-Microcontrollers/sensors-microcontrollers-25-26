# globals.py
# Central configuration file for the racing data system
# Defines shared memory layout, sensor names, and system constants
#
# Abdi
# Updated: January 2026

import numpy as np

# ==================== SHARED MEMORY CONFIG ====================
SHMEM_NAME = "mem123"           # Name of shared memory block
SHMEM_DTYPE = np.float32        # Data type for all sensor values
SHMEM_MEMB_SIZE = np.dtype(SHMEM_DTYPE).itemsize  # 4 bytes per float32

# ==================== SENSOR DEFINITIONS ====================
# Order matters! Index in this list = index in shared memory
SENS_NAMES = [
    # Steering & IMU (indices 0-9)
    "SteeringWheel",     # 0
    "IMUAccelX",         # 1
    "IMUAccelY",         # 2
    "IMUAccelZ",         # 3
    "IMUGyroX",          # 4
    "IMUGyroY",          # 5
    "IMUGyroZ",          # 6
    "IMUMagnetX",        # 7
    "IMUMagnetY",        # 8
    "IMUMagnetZ",        # 9
    
    # Wheel Speeds (indices 10-13) - Reserved for future
    "WSFrontLeft",       # 10
    "WSFrontRight",      # 11
    "WSBackLeft",        # 12
    "WSBackRight",       # 13
    
    # Motor Controller Data (indices 14-29)
    "MotorPlaceholder",  # 14 - Reserved
    "MotorCBTemp",       # 15 - Control Board Temperature
    "MotorCoolantTemp",  # 16 - Coolant Temperature
    "MotorHeatsinkTemp", # 17 - Heatsink Temperature
    "MotorTemp",         # 18 - Motor Temperature
    "Pedal1Position",    # 19 - Pedal 1 Position
    "Pedal2Position",    # 20 - Pedal 2 Position
    "MotorAngle",        # 21 - Motor Angle
    "MotorSpeed",        # 22 - Motor Speed (RPM)
    "DCCurrent",         # 23 - DC Bus Current
    "DCVoltage",         # 24 - DC Bus Voltage
    "VSMState",          # 25 - Vehicle State Machine State
    "InverterState",     # 26 - Inverter State
    "Direction",         # 27 - Forward/Reverse
    "Torque",            # 28 - Motor Torque
    "Timer",             # 29 - Internal Timer
    
    # BMS Data (indices 30-37)
    "BMSVolt1",          # 30 - Voltage sample 1
    "BMSVolt2",          # 31 - Voltage sample 2
    "BMSRes1",           # 32 - Resistance sample 1
    "BMSRes2",           # 33 - Resistance sample 2
    "BMSOV1",            # 34 - Open voltage sample 1
    "BMSOV2",            # 35 - Open voltage sample 2
    "BMSResAvg",         # 36 - Average resistance
    "BMSOVAvg",          # 37 - Average open voltage
]

SHMEM_NMEM = len(SENS_NAMES)
SHMEM_TOTAL_SIZE = SHMEM_NMEM * SHMEM_MEMB_SIZE  # Total bytes needed

# ==================== INDEX HELPERS ====================
MOTOR_START_IDX = SENS_NAMES.index("MotorPlaceholder")  # 14
BMS_START_IDX = SENS_NAMES.index("BMSVolt1")            # 30

# ==================== CAN BUS CONFIG ====================
CAN_INTERFACE = "vcan0"          # SocketCAN interface name
CAN_BITRATE = 500000            # 500 kbps
CAN_CHANNEL = "canusb_data"     # Legacy - not used without Redis

# ==================== DISPLAY CONFIG ====================
DISPLAY_UPDATE_RATE_MS = 100    # Update display every 100ms
DISPLAY_WIDTH = 800             # Display resolution
DISPLAY_HEIGHT = 480

# ==================== LOGGING CONFIG ====================
LOG_PATH = "/media/sd_card/logs/"  # SD card mount point
LOG_RATE_HZ = 10                   # Log 10 samples per second

# ==================== USB PORTS (Legacy - for Arduino) ====================
PI_USB0 = "/dev/ttyACM0"
PI_USB1 = "/dev/ttyACM1"
PI_USB2 = "/dev/ttyACM2"
PI_USB3 = "/dev/ttyACM3"