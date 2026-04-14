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
import time
import threading 
import numpy as np

# Add config directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'config'))
from globals import *
from orion_bms_decoder import OrionBMSDecoder

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
    
    def __init__(self, interfaces=CAN_INTERFACES, bitrate=CAN_BITRATE):
        """
        Initialize CAN processor with shared memory.
        
        Args:
            interfaces: List of CAN interface names (default: ['can0'])
            bitrate: CAN bus bitrate (default: 500000)
        """
        #self.interfaces = interfaces
        #self.bitrate = bitrate
        self.running = True
        # Timestamp tracker: maps CAN ID → last time we saw it (perf_counter seconds)
        # We use a dict so lookups are O(1) regardless of how many IDs appear
        self._last_seen: dict[int, float] = {}
        
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
        self.bms_decoder = OrionBMSDecoder()
        # Initialize CAN bus
        # Store the list of interfaces we'll listen on
        self.interfaces = interfaces if isinstance(interfaces, list) else [interfaces]
        self.bitrate = bitrate

        # Initialize one Bus object per interface, storing them all in a list
        # so cleanup() can shut them all down later
        self.buses = []
        for iface in self.interfaces:
            try:
                bus = can.interface.Bus(
                    channel=iface,
                    interface='socketcan'
                )
                self.buses.append(bus)
                print(f"✓ Connected to CAN interface '{iface}'")
            except Exception as e:
                print(f"✗ Failed to connect to CAN interface '{iface}': {e}")
                print("  Make sure that interface is configured:")
                print(f"    sudo ip link set {iface} type can bitrate {bitrate}")
                print(f"    sudo ip link set up {iface}")
                # Don't exit — if one bus fails, still try the others

        # If not a single bus connected, there's nothing to do
        if not self.buses:
            print("✗ No CAN interfaces could be connected. Exiting.")
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
        # the SENS_NAMES list in py for index mapping.
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
                # Each pedal is a 16-bit little-endian value
                pedal1_raw = struct.unpack('<H', data[0:2])[0]
                pedal2_raw = struct.unpack('<H', data[2:4])[0]
                 # Apply scaling (based on your generator)
                pedal1 = pedal1_raw / 1000.0
                pedal2 = pedal2_raw / 100.0 + 1.03
                self.data[MOTOR_START_IDX + 5] = pedal1
                self.data[MOTOR_START_IDX + 6] = pedal2
                
            elif can_id == 165:  # 0xA5 - Motor Position
                motor_angle = struct.unpack('<H', data[0:2])[0] / 10.0
                motor_speed = struct.unpack('<H', data[2:4])[0]
                self.data[MOTOR_START_IDX + 7] = motor_angle
                self.data[MOTOR_START_IDX + 8] = motor_speed
                print(f"Motor Angle: {motor_angle:.2f} deg, Speed: {motor_speed} RPM")
                
            elif can_id == 166:  # 0xA6 - Current Info
                dc_curr = struct.unpack('<H', data[6:8])[0] / 10.0
                self.data[MOTOR_START_IDX + 9] = dc_curr
                print(f"DC Current: {dc_curr:.2f} A")
                
            elif can_id == 167:  # 0xA7 - Voltage Info
                dc_volt = struct.unpack('<H', data[0:2])[0] / 10.0
                self.data[MOTOR_START_IDX + 10] = dc_volt
                print(f"DC Voltage: {dc_volt:.2f} V")
                
            elif can_id == 170:  # 0xAA - Internal States
                vsm_state = float(data[0])
                inv_state = float(data[2])
                direction = float(data[7] & 1)
                self.data[MOTOR_START_IDX + 11] = vsm_state
                self.data[MOTOR_START_IDX + 12] = inv_state
                self.data[MOTOR_START_IDX + 13] = direction
                print(f"VSM State: {vsm_state:.0f}, Inverter State: {inv_state:.0f}, Direction: {'Forward' if direction == 0 else 'Reverse'}")
            elif can_id == 171:  # 0xAB - Fault Codes
                post_fault_lo = struct.unpack('<H', data[0:2])[0]
                post_fault_hi = struct.unpack('<H', data[2:4])[0]
                run_fault_lo  = struct.unpack('<H', data[4:6])[0]
                run_fault_hi  = struct.unpack('<H', data[6:8])[0]
                self.data[BMS_START_IDX + 4] = float(post_fault_lo)
                self.data[BMS_START_IDX + 5] = float(post_fault_hi)
                self.data[BMS_START_IDX + 6] = float(run_fault_lo)
                self.data[BMS_START_IDX + 7] = float(run_fault_hi)
                if run_fault_lo or run_fault_hi:
                        print(f"⚠️  FAULT: run_lo=0x{run_fault_lo:04X} run_hi=0x{run_fault_hi:04X}")
            elif can_id == 172:  # 0xAC - Torque & Timer
                torque = struct.unpack('<H', data[0:2])[0] / 10.0
                timer = struct.unpack('<H', data[2:4])[0]
                self.data[MOTOR_START_IDX + 14] = torque
                self.data[MOTOR_START_IDX + 15] = timer
                print(f"Torque: {torque:.2f} Nm, Timer: {timer} ms")
                
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
            
    def _log_latency(self, can_id: int):
        """
        Called every time a message arrives. Computes and prints the time
        since the last message with this same CAN ID.
        
        perf_counter() returns seconds as a float, so we multiply by 1000
        to get milliseconds which is a more natural unit for CAN timing.
        """
        now = time.perf_counter()
        
        if can_id in self._last_seen:
            delta_ms = (now - self._last_seen[can_id]) * 1000.0
            print(f"[LATENCY] ID 0x{can_id:03X} ({can_id:3d}) → {delta_ms:7.2f} ms since last")
        else:
            # First time we've seen this ID — nothing to compare against yet
            print(f"[LATENCY] ID 0x{can_id:03X} ({can_id:3d}) → first message seen")
        
        # Always update the timestamp after logging
        self._last_seen[can_id] = now
    def process_message(self, msg):
        """
        Process a single CAN message and route to appropriate parser.
        
        Args:
            msg: python-can Message object
        """
        can_id = msg.arbitration_id
        data = msg.data
        self._log_latency(can_id)
        
        # Route message based on ID range
        if can_id >= 160 and can_id <= 172:
            # Motor Controller messages
            self.parse_motor_controller(can_id, data)
        elif can_id in (0x6B0, 0x6B1, 0x6B2, 0x6B3):
             # Orion BMS pack-level messages (IDs 1712-1715)
            print(f"CAN ID: {hex(can_id)} DATA: {msg.data.hex()}")
            decoded = self.bms_decoder.decode(msg)
            if decoded:
                self.data[BMS_START_IDX + 0] = decoded["pack_voltage"]
                self.data[BMS_START_IDX + 1] = decoded["pack_current"]
                self.data[BMS_START_IDX + 2] = decoded["soc"]
                self.data[BMS_START_IDX + 3] = decoded["max_temp"]
            print(f"Decoded BMS Data: Voltage={decoded['pack_voltage']} V, Current={decoded['pack_current']} A, SOC={decoded['soc']} %, Max Temp={decoded['max_temp']} °C")
        elif 0 <= can_id <= 100:
            # STM32 test range — log everything so you can see what's being sent
            # We don't parse these yet, but we don't silently drop them either.
            # The struct.unpack calls here give you a quick look at the raw values
            # interpreted as little-endian unsigned shorts (2 bytes each = 4 values)
            try:
                vals = struct.unpack('<HHHH', data[0:8])
                print(f"[STM32]  ID 0x{can_id:03X} ({can_id:3d}) | "
                      f"raw bytes: {data.hex()} | "
                      f"as u16 LE: {vals[0]} {vals[1]} {vals[2]} {vals[3]}")
            except struct.error:
                # Message shorter than 8 bytes — just print the raw hex
                print(f"[STM32]  ID 0x{can_id:03X} ({can_id:3d}) | "
                      f"raw bytes: {data.hex()} (only {len(data)} bytes)")
        elif can_id < 160:
        # Old per-cell BMS format — not used with Orion, silently ignore
                print(f"[BMS-CELL] ID 0x{can_id:03X} DATA: {data.hex()}")
        else:
             # Completely unknown ID — log it so nothing is invisible during testing
            print(f"[UNKNOWN] ID 0x{can_id:03X} ({can_id}) DATA: {data.hex()}")
        # Ignore other IDs

    def _listen_on_bus(self, bus):
        """
        Listener loop for a single CAN bus. This runs in its own thread.
        
        The key insight is that process_message() routes purely by arbitration ID,
        so it doesn't matter which physical bus a message came from — both threads
        funnel into the same shared memory. Python's GIL ensures individual
        float32 writes to numpy are atomic enough for a live dashboard display.
        
        Args:
            bus: A python-can Bus object already connected to an interface
        """
        print(f"✓ Listener thread started for {bus.channel_info}")
        try:
            while self.running:
                # recv() with a timeout so the thread checks self.running periodically
                # instead of blocking forever if the bus goes quiet
                msg = bus.recv(timeout=1.0)
                if msg is not None:
                    self.process_message(msg)
        except Exception as e:
            # Log but don't crash — the other bus thread keeps running
            print(f"✗ Error in listener for {bus.channel_info}: {e}")

    def run(self):
        """
        Main loop. Spawns one listener thread per CAN interface so that
        can0 and can1 are read concurrently. The main thread then just
        waits for all listener threads to finish (which happens when
        self.running goes False on SIGTERM/SIGINT).
        """
        print(f"✓ CAN Processor running on {[b.channel_info for b in self.buses]}...")

        # Create and start one daemon thread per bus
        # Daemon threads are automatically killed when the main thread exits,
        # which is a safety net if cleanup() somehow doesn't reach them
        threads = []
        for bus in self.buses:
            t = threading.Thread(
                target=self._listen_on_bus,
                args=(bus,),
                daemon=True
            )
            t.start()
            threads.append(t)

        try:
            # Block the main thread until a keyboard interrupt or signal arrives
            while self.running:
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("\n✓ Keyboard interrupt received, shutting down...")
            self.running = False
        finally:
            # Signal threads to stop and wait for them to finish their current recv()
            self.running = False
            for t in threads:
                t.join(timeout=3.0)   # Give each thread up to 3s to exit cleanly
            self.cleanup()

    def cleanup(self):
        """Clean up resources"""
        print("✓ Cleaning up...")
        
        if hasattr(self, 'buses'):
            for bus in self.buses:
                try:
                    bus.shutdown()
                except Exception:
                    pass
            print("  • All CAN buses closed")
        
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