#!/usr/bin/env python3
# can_getter.py
# Purpose: Clean API for reading CAN sensor values from shared memory
# This provides helper methods for accessing specific sensor readings
#
# Architecture:
#   Qt Display / SD Logger → can_getter → Shared Memory ← can_processor
#
# Usage:
#   from can_getter import CANGetter
#
#   can = CANGetter()
#   speed = can.get_motor_speed()
#   temp  = can.get_motor_temp()
#   all_data = can.get_all_values()
#   can.close()
#
# Abdi
# January 2026

import sys
import os
from multiprocessing import shared_memory
import numpy as np

# Add config directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'config'))
from globals import *


class CANGetter:
    """
    Thread-safe reader for CAN sensor data from shared memory.
    Provides convenient methods to access specific sensor values.
    """

    def __init__(self):
        """
        Connect to existing shared memory created by can_processor.
        Raises FileNotFoundError if shared memory doesn't exist.
        """
        try:
            self.shm = shared_memory.SharedMemory(name=SHMEM_NAME)
            self.data = np.ndarray(
                shape=(SHMEM_NMEM,),
                dtype=SHMEM_DTYPE,
                buffer=self.shm.buf
            )
        except FileNotFoundError:
            raise FileNotFoundError(
                f"Shared memory '{SHMEM_NAME}' not found. "
                "Make sure can_processor.py is running first."
            )

    # ==================== MOTOR CONTROLLER GETTERS ====================

    def get_motor_cb_temp(self):
        """Get motor control board temperature in Celsius"""
        return float(self.data[MOTOR_START_IDX + 1])

    def get_motor_coolant_temp(self):
        """Get motor coolant temperature in Celsius"""
        return float(self.data[MOTOR_START_IDX + 2])

    def get_motor_heatsink_temp(self):
        """Get motor heatsink temperature in Celsius"""
        return float(self.data[MOTOR_START_IDX + 3])

    def get_motor_temp(self):
        """Get motor temperature in Celsius"""
        return float(self.data[MOTOR_START_IDX + 4])

    def get_pedal1_position(self):
        """Get pedal 1 position (0.0 - 1.0)"""
        return float(self.data[MOTOR_START_IDX + 5])

    def get_pedal2_position(self):
        """Get pedal 2 position (0.0 - 1.0)"""
        return float(self.data[MOTOR_START_IDX + 6])

    def get_motor_angle(self):
        """Get motor angle in degrees"""
        return float(self.data[MOTOR_START_IDX + 7])

    def get_motor_speed(self):
        """Get motor speed in RPM"""
        return float(self.data[MOTOR_START_IDX + 8])

    def get_dc_current(self):
        """Get DC bus current in Amps"""
        return float(self.data[MOTOR_START_IDX + 9])

    def get_dc_voltage(self):
        """Get DC bus voltage in Volts"""
        return float(self.data[MOTOR_START_IDX + 10])

    def get_vsm_state(self):
        """Get Vehicle State Machine state"""
        return int(self.data[MOTOR_START_IDX + 11])

    def get_inverter_state(self):
        """Get inverter state"""
        return int(self.data[MOTOR_START_IDX + 12])

    def get_direction(self):
        """Get vehicle direction (0=reverse, 1=forward)"""
        return int(self.data[MOTOR_START_IDX + 13])

    def get_torque(self):
        """Get motor torque in Nm"""
        return float(self.data[MOTOR_START_IDX + 14])

    def get_timer(self):
        """Get internal timer value"""
        return int(self.data[MOTOR_START_IDX + 15])

    # ==================== BMS GETTERS (Orion BMS 2) ====================

    def get_pack_voltage(self):
        """Get BMS pack instantaneous voltage in Volts (from 0xC0)"""
        return float(self.data[BMS_START_IDX + 0])

    def get_bms_soc(self):
        """
        Get battery State of Charge in percent (0.0 – 100.0).
        Source: 0xC0 byte 6, scaled 0.5 %/bit, clamped at 100 %.
        """
        return float(self.data[BMS_START_IDX + 2])

    def get_bms_high_temp(self):
        """
        Get highest cell temperature in Celsius (from 0xC1 byte 1).
        Range: −40 °C to +80 °C.
        """
        return float(self.data[BMS_START_IDX + 3])

    def get_bms_low_temp(self):
        """
        Get lowest cell temperature in Celsius (from 0xC1 byte 0).
        Range: −40 °C to +80 °C.
        """
        return float(self.data[BMS_START_IDX + 4])

    def get_bms_fault_active(self):
        """
        Returns True if the BMS has signalled a fault via 0xC2 (byte 5 == 0x2A).
        Returns False when no fault is present.
        """
        return bool(self.data[BMS_START_IDX + 5])

    def get_mc_fault_codes(self):
        """
        Returns (run_fault_lo, run_fault_hi) for motor controller run-time faults.
        Both are 0 when no fault is active.
        Source: MC CAN message 0xAB bytes 4-7.
        """
        return (
            int(self.data[BMS_START_IDX + 6]),   # MCFaultRunLo
            int(self.data[BMS_START_IDX + 7]),   # MCFaultRunHi
        )

    def has_any_fault(self):
        """
        Returns True if either the BMS or the motor controller reports a fault.
        Convenience wrapper used by test scripts.
        """
        bms_fault = self.get_bms_fault_active()
        mc_lo, mc_hi = self.get_mc_fault_codes()
        return bms_fault or mc_lo != 0 or mc_hi != 0

    # ==================== IMU GETTERS ====================

    def get_imu_accel_x(self):
        return float(self.data[1])

    def get_imu_accel_y(self):
        return float(self.data[2])

    def get_imu_accel_z(self):
        return float(self.data[3])

    def get_imu_gyro_x(self):
        return float(self.data[4])

    def get_imu_gyro_y(self):
        return float(self.data[5])

    def get_imu_gyro_z(self):
        return float(self.data[6])

    # ==================== GENERAL GETTERS ====================

    def get_steering_wheel(self):
        """Get steering wheel angle"""
        return float(self.data[0])

    def get_value_by_index(self, index):
        if 0 <= index < SHMEM_NMEM:
            return float(self.data[index])
        raise IndexError(f"Index {index} out of range (0-{SHMEM_NMEM-1})")

    def get_value_by_name(self, name):
        if name in SENS_NAMES:
            return float(self.data[SENS_NAMES.index(name)])
        raise ValueError(f"Unknown sensor name: {name}")

    def get_all_values(self):
        return {name: float(self.data[i]) for i, name in enumerate(SENS_NAMES)}

    def get_all_motor_values(self):
        return {
            SENS_NAMES[i]: float(self.data[i])
            for i in range(MOTOR_START_IDX, BMS_START_IDX)
        }

    def get_all_bms_values(self):
        return {
            SENS_NAMES[i]: float(self.data[i])
            for i in range(BMS_START_IDX, SHMEM_NMEM)
        }

    # ==================== CALCULATED VALUES ====================

    def get_power(self):
        """Electrical power = DC Voltage × DC Current (Watts)"""
        return self.get_dc_voltage() * self.get_dc_current()

    def get_speed_mph(self, wheel_diameter_inches=20, gear_ratio=10.5):
        """
        Vehicle speed in MPH from motor RPM.
        Defaults: 20-inch wheel, 10.5:1 gear ratio.
        """
        rpm = self.get_motor_speed()
        wheel_rpm = rpm / gear_ratio
        wheel_circumference_ft = (wheel_diameter_inches * 3.14159) / 12
        speed_fpm = wheel_rpm * wheel_circumference_ft
        return speed_fpm * 60 / 5280

    def get_direction_text(self):
        return 'Forward' if self.get_direction() == 1 else 'Reverse'

    # ==================== UTILITY METHODS ====================

    def is_data_valid(self):
        """Returns True if pack voltage is positive (basic sanity check)."""
        voltage = self.get_pack_voltage()
        print(f"Pack Voltage: {voltage:.1f} V")
        return voltage > 0

    def print_summary(self):
        """Print a summary of key sensor values (useful for debugging)"""
        print("=" * 60)
        print("             CAN Data Summary")
        print("=" * 60)
        print(f"Motor Speed:      {self.get_motor_speed():.1f} RPM")
        print(f"Motor Temp:       {self.get_motor_temp():.1f} °C")
        print(f"DC Voltage:       {self.get_dc_voltage():.1f} V")
        print(f"DC Current:       {self.get_dc_current():.1f} A")
        print(f"Power:            {self.get_power():.0f} W")
        print(f"Torque:           {self.get_torque():.1f} Nm")
        print(f"Pedal Position:   {self.get_pedal1_position():.1%}")
        print(f"Direction:        {self.get_direction_text()}")
        print(f"Pack Voltage:     {self.get_pack_voltage():.1f} V")
        print(f"SOC:              {self.get_bms_soc():.1f} %")
        print(f"Batt High Temp:   {self.get_bms_high_temp():.1f} °C")
        print(f"Batt Low  Temp:   {self.get_bms_low_temp():.1f} °C")
        print(f"BMS Fault:        {'ACTIVE' if self.get_bms_fault_active() else 'none'}")
        mc_lo, mc_hi = self.get_mc_fault_codes()
        print(f"MC Run Fault:     0x{mc_lo:04X} / 0x{mc_hi:04X}")
        print("=" * 60)

    def close(self):
        """Close shared memory connection"""
        if hasattr(self, 'shm'):
            self.shm.close()


# ==================== EXAMPLE USAGE ====================
def main():
    try:
        can = CANGetter()
        print("✓ Connected to shared memory")
        import time
        while True:
            can.print_summary()
            time.sleep(0.5)
    except FileNotFoundError as e:
        print(f"✗ Error: {e}")
        print("  Start can_processor.py first!")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n✓ Exiting...")
    finally:
        if 'can' in locals():
            can.close()


if __name__ == "__main__":
    main()
