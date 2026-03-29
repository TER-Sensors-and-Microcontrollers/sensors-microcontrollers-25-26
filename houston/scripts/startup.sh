
# wait until IP is assigned
while ! hostname -I > /dev/null 2>&1; do
    echo "Waiting for network..."
    sleep 1
done
echo "Starting processes..."
python3 ~/sensors-microcontrollers/houston/serial-reader.py & 
python3 ~/sensors-microcontrollers/houston/app.py &