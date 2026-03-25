#!/bin/bash
# start_on_boot.sh
# Runs at boot: activates venv, cleans shared memory, starts CAN + display

PROJECT_ROOT="/home/racing/sensors-microcontrollers-25-26/onboard-rpi/racingDisplay"

# Clean stale shared memory from last session
sudo rm -f /dev/shm/mem123

# Activate venv
source "$PROJECT_ROOT/venv/bin/activate"

# Start CAN processor in background
python3 "$PROJECT_ROOT/core/can_processor.py" &
CAN_PID=$!
echo "CAN processor started (PID: $CAN_PID)"

# Wait for shared memory to initialize
sleep 2

# Start QML display (needs DISPLAY set for the screen)
export DISPLAY=:0
python3 "$PROJECT_ROOT/display/qml_display.py" &
DISPLAY_PID=$!
echo "QML display started (PID: $DISPLAY_PID)"

# Keep script alive so systemd tracks the process group
wait