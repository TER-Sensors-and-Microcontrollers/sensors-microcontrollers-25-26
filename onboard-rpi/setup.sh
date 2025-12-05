#!/bin/bash

# --- Virtual CAN Interface Setup ---
echo "Setting up VCAN interface vcan0..."
# Note: These commands require the container to be run with the 
# '--privileged' flag or specific capabilities enabled.

# 1. Load the vcan kernel module (if not already loaded)
# Note: 'modprobe' is typically not available or not necessary inside a container, 
# as the module needs to be loaded on the host. We use the ip link commands anyway 
# as the vcan module is often implicitly loaded or assumed to be on the host kernel.

# 2. Add the virtual CAN link (vcan0)
# We use 'ip' with the full path and without sudo, relying on Docker's capabilities
/sbin/ip link add dev vcan0 type vcan 

# 3. Set the link up
/sbin/ip link set up vcan0 

echo "VCAN interface vcan0 is now ready."
echo "You can send test data using: candump vcan0"

# --- Start the CAN Application ---
# Replace this command with the actual command to launch your Qt application
# For now, we will just launch a shell so you can manually test the VCAN setup.
exec /bin/bash