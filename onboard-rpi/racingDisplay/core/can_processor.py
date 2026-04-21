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
import time
import threading
import numpy as np
from multiprocessing import shared_memory

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

    Message routing:
      0xA0-0xAC  Motor Controller (remap: 160-172 decimal)
      0xC0       Orion BMS 2 — Pack Voltage + SOC
      0xC1       Orion BMS 2 — Cell Temperatures (Low / High)
      0xC2       Orion BMS 2 — Fault Codes
      0x036      Per-cell messages — logged, not parsed
      0-100      STM32 test range — logged, not stored
    """

    def __init__(self, interfaces=CAN_INTERFACES, bitrate=CAN_BITRATE):
        self.running = True
        self._last_seen: dict[int, float] = {}

        signal.signal(signal.SIGTERM, self._signal_handler)
        signal.signal(signal.SIGINT,  self._signal_handler)

        # ---- Shared memory ----
        try:
            self.shm = shared_memory.SharedMemory(
                create=True, size=SHMEM_TOTAL_SIZE, name=SHMEM_NAME
            )
            print(f"✓ Created shared memory '{SHMEM_NAME}' ({SHMEM_TOTAL_SIZE} bytes)")
        except FileExistsError:
            self.shm = shared_memory.SharedMemory(name=SHMEM_NAME)
            print(f"✓ Attached to existing shared memory '{SHMEM_NAME}'")

        self.data = np.ndarray(shape=SHMEM_NMEM, dtype=SHMEM_DTYPE, buffer=self.shm.buf)
        self.data[:] = 0

        # ---- BMS decoder ----
        self.bms_decoder = OrionBMSDecoder()

        # ---- CAN interfaces ----
        self.interfaces = interfaces if isinstance(interfaces, list) else [interfaces]
        self.bitrate = bitrate
        self.buses = []

        for iface in self.interfaces:
            try:
                bus = can.interface.Bus(channel=iface, interface='socketcan')
                self.buses.append(bus)
                print(f"✓ Connected to CAN interface '{iface}'")
            except Exception as e:
                print(f"✗ Failed to connect to '{iface}': {e}")
                print(f"    sudo ip link set {iface} type can bitrate {bitrate}")
                print(f"    sudo ip link set up {iface}")

        if not self.buses:
            print("✗ No CAN interfaces could be connected. Exiting.")
            self.cleanup()
            sys.exit(1)

    # ------------------------------------------------------------------
    def _signal_handler(self, signum, frame):
        print(f"\n✓ Received signal {signum}, shutting down...")
        self.running = False

    # ------------------------------------------------------------------
    def parse_motor_controller(self, can_id, data):
        """
        Parse Motor Controller CAN messages (IDs 0xA0-0xAC / 160-172).

        IDs handled:
          161 (0xA1) — Control board temp
          162 (0xA2) — Coolant / heatsink / motor temps
          163 (0xA3) — Pedal positions
          165 (0xA5) — Motor angle + speed
          166 (0xA6) — DC current
          167 (0xA7) — DC voltage
          170 (0xAA) — State machine states
          171 (0xAB) — Run-time fault codes
          172 (0xAC) — Torque + timer
        """
        if len(data) < 8:
            return

        try:
            if can_id == 161:   # 0xA1 — Temp 2 (Control Board)
                cb_temp = struct.unpack('<H', data[0:2])[0] / 10.0
                self.data[MOTOR_START_IDX + 1] = cb_temp

            elif can_id == 162:  # 0xA2 — Temp 3 (Coolant / Heatsink / Motor)
                cool_temp   = struct.unpack('<H', data[0:2])[0] / 10.0
                htspt_temp  = struct.unpack('<H', data[2:4])[0] / 10.0
                mot_temp    = struct.unpack('<H', data[4:6])[0] / 10.0
                self.data[MOTOR_START_IDX + 2] = cool_temp
                self.data[MOTOR_START_IDX + 3] = htspt_temp
                self.data[MOTOR_START_IDX + 4] = mot_temp

            elif can_id == 163:  # 0xA3 — Pedal positions (LE uint16 per pedal)
                pedal1 = struct.unpack('<H', data[0:2])[0] / 1000.0
                pedal2 = struct.unpack('<H', data[2:4])[0] / 100.0 + 1.03
                self.data[MOTOR_START_IDX + 5] = pedal1
                self.data[MOTOR_START_IDX + 6] = pedal2

            elif can_id == 165:  # 0xA5 — Motor position
                motor_angle = struct.unpack('<H', data[0:2])[0] / 10.0
                motor_speed = struct.unpack('<H', data[2:4])[0]
                self.data[MOTOR_START_IDX + 7] = motor_angle
                self.data[MOTOR_START_IDX + 8] = motor_speed
                print(f"Motor  Angle={motor_angle:.1f}°  Speed={motor_speed} RPM")

            elif can_id == 166:  # 0xA6 — DC current
                dc_curr = struct.unpack('<H', data[6:8])[0] / 10.0
                self.data[MOTOR_START_IDX + 9] = dc_curr
                print(f"DC Current={dc_curr:.1f} A")

            elif can_id == 167:  # 0xA7 — DC voltage
                dc_volt = struct.unpack('<H', data[0:2])[0] / 10.0
                self.data[MOTOR_START_IDX + 10] = dc_volt
                print(f"DC Voltage={dc_volt:.1f} V")

            elif can_id == 170:  # 0xAA — Internal states
                vsm_state = float(data[0])
                inv_state  = float(data[2])
                direction  = float(data[7] & 1)
                self.data[MOTOR_START_IDX + 11] = vsm_state
                self.data[MOTOR_START_IDX + 12] = inv_state
                self.data[MOTOR_START_IDX + 13] = direction
                print(f"VSM={vsm_state:.0f}  Inv={inv_state:.0f}  Dir={'FWD' if direction else 'REV'}")

            elif can_id == 171:  # 0xAB — Fault codes
                # Only run-time faults are stored (POST/boot faults dropped
                # to free shared memory for BMS data).
                run_fault_lo = struct.unpack('<H', data[4:6])[0]
                run_fault_hi = struct.unpack('<H', data[6:8])[0]
                self.data[BMS_START_IDX + 6] = float(run_fault_lo)  # MCFaultRunLo
                self.data[BMS_START_IDX + 7] = float(run_fault_hi)  # MCFaultRunHi
                if run_fault_lo or run_fault_hi:
                    print(f"⚠️  MC FAULT run_lo=0x{run_fault_lo:04X} run_hi=0x{run_fault_hi:04X}")

            elif can_id == 172:  # 0xAC — Torque + timer
                torque = struct.unpack('<H', data[0:2])[0] / 10.0
                timer  = struct.unpack('<H', data[2:4])[0]
                self.data[MOTOR_START_IDX + 14] = torque
                self.data[MOTOR_START_IDX + 15] = timer
                print(f"Torque={torque:.1f} Nm  Timer={timer}")

        except struct.error as e:
            print(f"Warning: Failed to parse motor controller ID {can_id}: {e}")

    # ------------------------------------------------------------------
    def parse_orion_bms(self, msg):
        """
        Parse Orion BMS 2 messages (0xC0 / 0xC1 / 0xC2) and write
        all accumulated BMS fields to shared memory in one pass.

        Writing all five fields on every BMS message is intentional:
        the decoder accumulates state, so stale fields simply keep their
        last-known values — acceptable for a live dashboard.
        """
        decoded = self.bms_decoder.decode(msg)

        # --- Write to shared memory ---
        self.data[BMS_START_IDX + 0] = decoded["pack_voltage"]            # BMSPackVoltage
        # BMS_START_IDX + 1 (BMSPackCurrent) left at 0 — not yet decoded
        self.data[BMS_START_IDX + 2] = decoded["soc"]                     # BMSSOC
        self.data[BMS_START_IDX + 3] = decoded["high_temp"]               # BMSHighTemp
        self.data[BMS_START_IDX + 4] = decoded["low_temp"]                # BMSLowTemp
        self.data[BMS_START_IDX + 5] = 1.0 if decoded["fault_active"] else 0.0  # BMSFaultActive

        # --- Targeted debug print per message type ---
        can_id = msg.arbitration_id
        if can_id == 0xC0:
            print(f"[BMS 0xC0] Voltage={decoded['pack_voltage']:.1f} V  SOC={decoded['soc']:.1f}%")
        elif can_id == 0xC1:
            print(f"[BMS 0xC1] HighTemp={decoded['high_temp']:.0f}°C  LowTemp={decoded['low_temp']:.0f}°C")
        elif can_id == 0xC2:
            state = "ACTIVE" if decoded["fault_active"] else "none"
            print(f"[BMS 0xC2] Fault={state}")

    # ------------------------------------------------------------------
    def _log_latency(self, can_id: int):
        """Log inter-message timing for a given CAN ID."""
        now = time.perf_counter()
        if can_id in self._last_seen:
            delta_ms = (now - self._last_seen[can_id]) * 1000.0
            print(f"[LATENCY] ID 0x{can_id:03X} ({can_id:3d}) → {delta_ms:7.2f} ms since last")
        else:
            print(f"[LATENCY] ID 0x{can_id:03X} ({can_id:3d}) → first message seen")
        self._last_seen[can_id] = now

    # ------------------------------------------------------------------
    def process_message(self, msg):
        """Route one CAN message to the correct parser."""
        can_id = msg.arbitration_id
        data   = msg.data
        self._log_latency(can_id)

        if 160 <= can_id <= 172:
            # Motor Controller
            self.parse_motor_controller(can_id, data)

        elif can_id in (0xC0, 0xC1, 0xC2):
            # Orion BMS 2 pack-level messages
            self.parse_orion_bms(msg)

        elif 0 <= can_id <= 100:
            # STM32 test range — log raw bytes, do not store
            try:
                vals = struct.unpack('<HHHH', data[0:8])
                print(f"[STM32]  ID 0x{can_id:03X} | {data.hex()} | u16LE: {vals}")
            except struct.error:
                print(f"[STM32]  ID 0x{can_id:03X} | {data.hex()} ({len(data)} B)")

        elif can_id < 160:
            # Per-cell BMS messages (0x036 etc.) — log only, not stored
            print(f"[BMS-CELL] ID 0x{can_id:03X} | {data.hex()}")

        else:
            # Unknown ID — log so nothing is invisible during testing
            print(f"[UNKNOWN] ID 0x{can_id:03X} ({can_id}) | {data.hex()}")

    # ------------------------------------------------------------------
    def _listen_on_bus(self, bus):
        """Listener loop for one CAN interface — runs in its own thread."""
        print(f"✓ Listener thread started for {bus.channel_info}")
        try:
            while self.running:
                msg = bus.recv(timeout=1.0)
                if msg is not None:
                    self.process_message(msg)
        except Exception as e:
            print(f"✗ Error in listener for {bus.channel_info}: {e}")

    # ------------------------------------------------------------------
    def run(self):
        """
        Spawn one listener thread per CAN interface and block until
        SIGTERM / SIGINT is received.
        """
        print(f"✓ CAN Processor running on {[b.channel_info for b in self.buses]}...")

        threads = []
        for bus in self.buses:
            t = threading.Thread(target=self._listen_on_bus, args=(bus,), daemon=True)
            t.start()
            threads.append(t)

        try:
            while self.running:
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("\n✓ Keyboard interrupt received, shutting down...")
            self.running = False
        finally:
            self.running = False
            for t in threads:
                t.join(timeout=3.0)
            self.cleanup()

    # ------------------------------------------------------------------
    def cleanup(self):
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
            try:
                self.shm.unlink()
                print("  • Shared memory freed")
            except FileNotFoundError:
                print("  • Shared memory detached")


def main():
    print("=" * 60)
    print("         Electric Racing CAN Processor v2.0")
    print("=" * 60)
    processor = CANProcessor()
    processor.run()


if __name__ == "__main__":
    main()
