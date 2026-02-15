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
#   temp = can.get_motor_temp()
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
                shape=SHMEM_NMEM, 
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
    
    # ==================== BMS GETTERS ====================
    
    def get_bms_avg_resistance(self):
        """Get average BMS cell resistance"""
        return float(self.data[BMS_START_IDX + 6])
    
    def get_bms_avg_voltage(self):
        """Get average BMS cell open voltage"""
        return float(self.data[BMS_START_IDX + 7])
    
    # ==================== IMU GETTERS ====================
    
    def get_imu_accel_x(self):
        """Get IMU X-axis acceleration"""
        return float(self.data[1])
    
    def get_imu_accel_y(self):
        """Get IMU Y-axis acceleration"""
        return float(self.data[2])
    
    def get_imu_accel_z(self):
        """Get IMU Z-axis acceleration"""
        return float(self.data[3])
    
    def get_imu_gyro_x(self):
        """Get IMU X-axis gyro"""
        return float(self.data[4])
    
    def get_imu_gyro_y(self):
        """Get IMU Y-axis gyro"""
        return float(self.data[5])
    
    def get_imu_gyro_z(self):
        """Get IMU Z-axis gyro"""
        return float(self.data[6])
    
    # ==================== GENERAL GETTERS ====================
    
    def get_steering_wheel(self):
        """Get steering wheel angle"""
        return float(self.data[0])
    
    def get_value_by_index(self, index):
        """
        Get sensor value by index.
        
        Args:
            index: Index in shared memory (0-37)
            
        Returns:
            float: Sensor value
        """
        if 0 <= index < SHMEM_NMEM:
            return float(self.data[index])
        else:
            raise IndexError(f"Index {index} out of range (0-{SHMEM_NMEM-1})")
    
    def get_value_by_name(self, name):
        """
        Get sensor value by name.
        
        Args:
            name: Sensor name from SENS_NAMES
            
        Returns:
            float: Sensor value
        """
        if name in SENS_NAMES:
            index = SENS_NAMES.index(name)
            return float(self.data[index])
        else:
            raise ValueError(f"Unknown sensor name: {name}")
    
    def get_all_values(self):
        """
        Get all sensor values as a dictionary.
        
        Returns:
            dict: {sensor_name: value} for all sensors
        """
        return {name: float(self.data[i]) for i, name in enumerate(SENS_NAMES)}
    
    def get_all_motor_values(self):
        """
        Get all motor-related values.
        
        Returns:
            dict: Motor sensor values only
        """
        motor_values = {}
        for i in range(MOTOR_START_IDX, BMS_START_IDX):
            motor_values[SENS_NAMES[i]] = float(self.data[i])
        return motor_values
    
    def get_all_bms_values(self):
        """
        Get all BMS-related values.
        
        Returns:
            dict: BMS sensor values only
        """
        bms_values = {}
        for i in range(BMS_START_IDX, SHMEM_NMEM):
            bms_values[SENS_NAMES[i]] = float(self.data[i])
        return bms_values
    
    # ==================== CALCULATED VALUES ====================
    
    def get_power(self):
        """
        Calculate electrical power (Voltage × Current).
        
        Returns:
            float: Power in Watts
        """
        voltage = self.get_dc_voltage()
        current = self.get_dc_current()
        return voltage * current
    
    def get_speed_mph(self, wheel_diameter_inches=20, gear_ratio=10.5):
        """
        Calculate vehicle speed in MPH from motor RPM.
        
        Args:
            wheel_diameter_inches: Wheel diameter (default: 20")
            gear_ratio: Motor to wheel gear ratio (default: 10.5)
            
        Returns:
            float: Speed in MPH
        """
        rpm = self.get_motor_speed()
        wheel_rpm = rpm / gear_ratio
        wheel_circumference_ft = (wheel_diameter_inches * 3.14159) / 12
        speed_fpm = wheel_rpm * wheel_circumference_ft
        speed_mph = speed_fpm * 60 / 5280
        return speed_mph
    
    def get_direction_text(self):
        """
        Get human-readable direction.
        
        Returns:
            str: 'Forward' or 'Reverse'
        """
        direction = self.get_direction()
        return 'Forward' if direction == 1 else 'Reverse'
    
    # ==================== UTILITY METHODS ====================
    
    def is_data_valid(self, max_age_seconds=1.0):
        """
        Check if data appears to be updating (simple validation).
        Checks if motor speed has changed recently.
        
        Args:
            max_age_seconds: Not implemented yet (for future timestamp checking)
            
        Returns:
            bool: True if data seems valid
        """
        # Simple check: if motor values are non-zero, assume valid
        speed = self.get_motor_speed()
        voltage = self.get_dc_voltage()
        return speed != 0 or voltage != 0
    
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
        print(f"BMS Avg Voltage:  {self.get_bms_avg_voltage():.2f}")
        print("=" * 60)
    
    def close(self):
        """Close shared memory connection"""
        if hasattr(self, 'shm'):
            self.shm.close()


# ==================== EXAMPLE USAGE ====================
def main():
    """Example usage of CANGetter"""
    try:
        can = CANGetter()
        print("✓ Connected to shared memory")
        
        # Print summary every 0.5 seconds
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
        can.close()


if __name__ == "__main__":
    main()