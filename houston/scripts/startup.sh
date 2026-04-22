
# wait until IP is assigned
while ! hostname -I > /dev/null 2>&1; do
    echo "Waiting for network..."
    sleep 1
done
echo "My wlan0 ip address is: "
ip addr show wlan0 | awk '/inet / {print $2}' | cut -d/ -f1
echo "Starting processes..."
python3 ~/sensors-microcontrollers/houston/app.py &
python3 ~/sensors-microcontrollers/houston/serial-reader.py & 
