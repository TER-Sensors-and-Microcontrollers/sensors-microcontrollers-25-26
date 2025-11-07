import can

# Use "interface" instead of "bustype" to avoid the deprecation warning
bus = can.interface.Bus(interface="virtual", channel="vcan0", bitrate=500000)

print("Listening for CAN frames... (Ctrl+C to stop)")
try:
    for msg in bus:
        print(f"Received: ID=0x{msg.arbitration_id:X}, Data={msg.data.hex()}")
except KeyboardInterrupt:
    print("Stopped listening.")
