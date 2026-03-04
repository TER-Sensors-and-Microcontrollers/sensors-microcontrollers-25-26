#!/usr/bin/env python3
import can

# 🔧 Change this to the CAN ID you want to highlight
TARGET_ID = 0x100   # example

def main():
    print("=== RAW CAN DEBUG ===")
    print(f"Highlighting ID: {hex(TARGET_ID)}")
    print("Listening on can0...\n")

    try:
        bus = can.interface.Bus(channel='can0', bustype='socketcan')
    except Exception as e:
        print(f"Failed to connect to can0: {e}")
        return

    while True:
        msg = bus.recv()  # blocking
        if msg is None:
            continue

        can_id = msg.arbitration_id
        data_hex = ' '.join(f'{b:02X}' for b in msg.data)
        data_dec = ' '.join(str(b) for b in msg.data)

        if can_id == TARGET_ID:
            print("\n🔥🔥🔥 TARGET ID DETECTED 🔥🔥🔥")
            print(f">>> ID: {hex(can_id)} ({can_id}) <<<")
            print(f"HEX: {data_hex}")
            print(f"DEC: {data_dec}")
            print("-----------------------------\n")
        else:
            print(f"ID: {hex(can_id)}  |  {data_hex}")

if __name__ == "__main__":
    main()