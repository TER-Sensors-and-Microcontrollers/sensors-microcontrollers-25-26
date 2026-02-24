#!/usr/bin/env python3
# can_processor.py
# Purpose: Read CAN bus messages and write parsed data to shared memory
# This runs as a background service and continuously updates sensor values
#
# Architecture:
#   CAN Bus → python-can → parse by ID → write to shared memory
#
# Usage:
#   python3 can_processor.py
#   or run as systemd service: sudo systemctl start racing-can
#
# Abdi
# January 2026

import sys
import os
import signal
import struct
import random
from multiprocessing import shared_memory
import numpy as np

# Add config directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'config'))
from globals import *

try:
    import can
except ImportError:
    print("ERROR: python-can not installed. Run: pip install python-can")
    sys.exit(1)


class CANProcessor:
    """
    Reads CAN bus messages and writes parsed values to shared memory.
    Handles both Motor Controller (IDs 160-172) and BMS (IDs < 160) messages.
    """
    
    def __init__(self, interface=CAN_INTERFACE, bitrate=CAN_BITRATE):
        """
        Initialize CAN processor with shared memory.
        
        Args:
            interface: CAN interface name (default: 'can0')
            bitrate: CAN bus bitrate (default: 500000)
        """
        self.interface = interface
        self.bitrate = bitrate
        self.running = True
        
        # Setup signal handlers for clean shutdown
        signal.signal(signal.SIGTERM, self._signal_handler)
        signal.signal(signal.SIGINT, self._signal_handler)
        
        # Initialize shared memory
        try:
            self.shm = shared_memory.SharedMemory(
                create=True, 
                size=SHMEM_TOTAL_SIZE, 
                name=SHMEM_NAME
            )
            print(f"✓ Created shared memory '{SHMEM_NAME}' ({SHMEM_TOTAL_SIZE} bytes)")
        except FileExistsError:
            # Shared memory already exists, attach to it
            self.shm = shared_memory.SharedMemory(name=SHMEM_NAME)
            print(f"✓ Attached to existing shared memory '{SHMEM_NAME}'")
        
        # Create numpy array backed by shared memory
        self.data = np.ndarray(
            shape=SHMEM_NMEM, 
            dtype=SHMEM_DTYPE, 
            buffer=self.shm.buf
        )
        self.data[:] = 0  # Initialize all values to zero
        
        # Initialize CAN bus
        try:
            self.bus = can.interface.Bus(
                channel=self.interface, 
                interface='virtual'
            )
            print(f"✓ Connected to CAN interface '{self.interface}'")
        except Exception as e:
            print(f"✗ Failed to connect to CAN interface: {e}")
            print("  Make sure CAN interface is configured:")
            print(f"    sudo ip link set {self.interface} type can bitrate {self.bitrate}")
            print(f"    sudo ip link set up {self.interface}")
            self.cleanup()
            sys.exit(1)
    
    def _signal_handler(self, signum, frame):
        """Handle termination signals gracefully"""
        print(f"\n✓ Received signal {signum}, shutting down...")
        self.running = False
    
    def parse_motor_controller(self, can_id, data):
        """
        Parse Motor Controller CAN messages (IDs 0xA0 - 0xAC).
        
        Motor Controller sends data on IDs 160-172 (0xA0-0xAC):
        - 161: Control board temp
        - 162: Coolant, heatsink, motor temps
        - 163: Pedal positions
        - 165: Motor angle and speed
        - 166: DC current
        - 167: DC voltage
        - 170: State machine states
        - 172: Torque and timer
        """
        # The message format is assumed to always be 8 bytes
        if len(data) < 8:
            return  # Skip incomplete messages
        # associates can_ID to the correct index in shared memory and
        # parses the data accordingly. See self.data area is the same as 
        # the SENS_NAMES list in globals.py for index mapping.
        try:
            if can_id == 161:  # 0xA1 - Temp 2
                cb_temp = struct.unpack('<H', data[0:2])[0] / 10.0
                self.data[MOTOR_START_IDX + 1] = cb_temp
                
            elif can_id == 162:  # 0xA2 - Temp 3
                cool_temp = struct.unpack('<H', data[0:2])[0] / 10.0
                htspt_temp = struct.unpack('<H', data[2:4])[0] / 10.0
                mot_temp = struct.unpack('<H', data[4:6])[0] / 10.0
                self.data[MOTOR_START_IDX + 2] = cool_temp
                self.data[MOTOR_START_IDX + 3] = htspt_temp
                self.data[MOTOR_START_IDX + 4] = mot_temp
                
            elif can_id == 163:  # 0xA3 - Analog Input (Pedals)
                # Convert data to bit string for bit operations
                bit_string = ''.join(format(byte, '08b') for byte in data)
                pedal1_bits = bit_string[-10:]  # Last 10 bits
                pedal2_bits = bit_string[20:30]  # Bits 20-30
                
                pedal1 = int(pedal1_bits, 2) / 1000.0  # Convert to percentage
                pedal2 = int(pedal2_bits, 2) / 100.0 + 1.03
                
                self.data[MOTOR_START_IDX + 5] = pedal1
                self.data[MOTOR_START_IDX + 6] = pedal2
                
            elif can_id == 165:  # 0xA5 - Motor Position
                motor_angle = struct.unpack('<H', data[0:2])[0] / 10.0
                motor_speed = struct.unpack('<H', data[2:4])[0]
                self.data[MOTOR_START_IDX + 7] = motor_angle
                self.data[MOTOR_START_IDX + 8] = motor_speed
                
            elif can_id == 166:  # 0xA6 - Current Info
                dc_curr = struct.unpack('<H', data[6:8])[0] / 10.0
                self.data[MOTOR_START_IDX + 9] = dc_curr
                
            elif can_id == 167:  # 0xA7 - Voltage Info
                dc_volt = struct.unpack('<H', data[0:2])[0] / 10.0
                self.data[MOTOR_START_IDX + 10] = dc_volt
                
            elif can_id == 170:  # 0xAA - Internal States
                vsm_state = float(data[0])
                inv_state = float(data[2])
                direction = float(data[7] & 1)
                self.data[MOTOR_START_IDX + 11] = vsm_state
                self.data[MOTOR_START_IDX + 12] = inv_state
                self.data[MOTOR_START_IDX + 13] = direction
                
            elif can_id == 172:  # 0xAC - Torque & Timer
                torque = struct.unpack('<H', data[0:2])[0] / 10.0
                timer = struct.unpack('<H', data[2:4])[0]
                self.data[MOTOR_START_IDX + 14] = torque
                self.data[MOTOR_START_IDX + 15] = timer
                
        except struct.error as e:
            print(f"Warning: Failed to parse motor controller ID {can_id}: {e}")
    
    def parse_bms(self, can_id, data):
        """
        Parse BMS CAN messages (IDs < 160).
        
        BMS sends cell voltage and resistance data.
        We sample random cells and store averages.
        """
        # The message data format is assumed to always be 8 bytes
        if len(data) < 8:
            return
        
        try:
            # Extract internal resistance and open voltage
            int_res = struct.unpack('<H', data[2:4])[0]
            open_volt = struct.unpack('<H', data[4:6])[0]
            
            # Store in random sampling cells (indices 32-35)
            # This gives us a statistical sample of all cells
            sample_res_idx = BMS_START_IDX + random.randint(2, 3)
            sample_ov_idx = BMS_START_IDX + random.randint(4, 5)
            
            self.data[sample_res_idx] = float(int_res)
            self.data[sample_ov_idx] = float(open_volt)
            
            # Calculate and store averages
            avg_res = (self.data[BMS_START_IDX + 2] + 
                      self.data[BMS_START_IDX + 3]) / 2.0
            avg_ov = (self.data[BMS_START_IDX + 4] + 
                     self.data[BMS_START_IDX + 5]) / 2.0
            
            self.data[BMS_START_IDX + 6] = avg_res
            self.data[BMS_START_IDX + 7] = avg_ov
            
        except struct.error as e:
            print(f"Warning: Failed to parse BMS ID {can_id}: {e}")
    
    def process_message(self, msg):
        """
        Process a single CAN message and route to appropriate parser.
        
        Args:
            msg: python-can Message object
        """
        can_id = msg.arbitration_id
        data = msg.data
        
        # Route message based on ID range
        if can_id >= 160 and can_id <= 172:
            # Motor Controller messages
            self.parse_motor_controller(can_id, data)
        elif can_id < 160:
            # BMS messages
            self.parse_bms(can_id, data)
        # Ignore other IDs
    
    def run(self):
        """
        Main loop.
        If no real CAN messages, generate fake test data.
        """
        print(f"✓ CAN Processor running (FAKE DATA MODE)...")
        print("  Press Ctrl+C to stop")

        t = 0.0
        print("✓ Generating fake data for testing...")
        try:
            while self.running:
                # ---- FAKE SPEED ----
                fake_speed =  4000 + 2000 * np.sin(t)
                self.data[MOTOR_START_IDX + 8] = fake_speed  # motor_speed index

                # ---- FAKE MOTOR TEMP ----
                fake_temp = 50 + 10 * np.sin(t / 2)
                self.data[MOTOR_START_IDX + 4] = fake_temp

                # ---- FAKE DC VOLTAGE ----
                fake_voltage = 300 + 5 * np.sin(t / 3)
                self.data[MOTOR_START_IDX + 10] = fake_voltage

                # ---- FAKE CURRENT ----
                fake_current = 100 + 30 * np.sin(t)
                self.data[MOTOR_START_IDX + 9] = fake_current
                print(f"Speed: {fake_speed:.2f}", end="\r")
                t += 0.1
                import time
                time.sleep(0.05)

        except KeyboardInterrupt:
            print("\n✓ Keyboard interrupt received, shutting down...")
            pass
        finally:
            self.cleanup()
    
    def cleanup(self):
        """Clean up resources"""
        print("✓ Cleaning up...")
        
        if hasattr(self, 'bus'):
            self.bus.shutdown()
            print("  • CAN bus closed")
        
        if hasattr(self, 'shm'):
            self.shm.close()
            # Only unlink if we created it
            try:
                self.shm.unlink()
                print("  • Shared memory freed")
            except FileNotFoundError:
                print("  • Shared memory detached")


def main():
    """Entry point"""
    print("=" * 60)
    print("         Electric Racing CAN Processor v2.0")
    print("=" * 60)
    
    processor = CANProcessor()
    processor.run()


if __name__ == "__main__":
    main()