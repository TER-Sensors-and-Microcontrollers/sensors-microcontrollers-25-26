#!/usr/bin/env python3
# test_system.py
# Purpose: Test the complete system by reading values from shared memory
# Useful for verifying that can_processor is working correctly
#
# Usage:
#   python3 test_system.py
#
# Abdi
# January 2026

import sys
import os
import time

# Add parent directories to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'core'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'config'))

from can_getter import CANGetter
from globals import SENS_NAMES

def test_connection():
    """Test if we can connect to shared memory"""
    print("=" * 60)
    print("  Electric Racing System Test")
    print("=" * 60)
    print()
    
    print("1. Testing shared memory connection...")
    try:
        can = CANGetter()
        print("   ✓ Connected to shared memory")
    except FileNotFoundError:
        print("   ✗ Failed to connect to shared memory")
        print("   Make sure can_processor.py is running!")
        return None
    
    return can

def test_values(can):
    """Test reading values from shared memory"""
    print()
    print("2. Testing value reading...")
    
    # Test individual getters
    tests = [
        ("Motor Speed", can.get_motor_speed, "RPM"),
        ("Motor Temp", can.get_motor_temp, "°C"),
        ("DC Voltage", can.get_dc_voltage, "V"),
        ("DC Current", can.get_dc_current, "A"),
        ("Power", can.get_power, "W"),
    ]
    
    for name, getter, unit in tests:
        try:
            value = getter()
            print(f"   {name:15} = {value:8.2f} {unit}")
        except Exception as e:
            print(f"   ✗ Error reading {name}: {e}")

def test_all_sensors(can):
    """Display all sensor values"""
    print()
    print("3. All sensor values:")
    print("-" * 60)
    
    all_values = can.get_all_values()
    
    for i, name in enumerate(SENS_NAMES):
        value = all_values[name]
        print(f"   [{i:2d}] {name:20} = {value:10.2f}")

def monitor_live(can, duration=10):
    """Monitor values live for a few seconds"""
    print()
    print(f"4. Live monitoring for {duration} seconds...")
    print("   (Watch for changing values)")
    print()
    
    start_time = time.time()
    last_speed = None
    changes = 0
    
    while time.time() - start_time < duration:
        speed = can.get_motor_speed()
        voltage = can.get_dc_voltage()
        temp = can.get_motor_temp()
        
        # Check if values are changing
        if last_speed is not None and last_speed != speed:
            changes += 1
        last_speed = speed
        
        # Display current values
        print(f"\r   Speed: {speed:6.1f} RPM  |  "
              f"Voltage: {voltage:5.1f} V  |  "
              f"Temp: {temp:5.1f} °C  |  "
              f"Changes: {changes}", end="")
        
        time.sleep(0.1)
    
    print()
    
    if changes > 0:
        print(f"   ✓ Detected {changes} value changes - CAN data is live!")
    else:
        print(f"   ⚠️  No changes detected - check CAN bus connection")

def test_data_validity(can):
    """Check if data appears valid"""
    print()
    print("5. Data validity check...")
    
    # Get some key values
    voltage = can.get_dc_voltage()
    speed = can.get_motor_speed()
    temp = can.get_motor_temp()
    
    issues = []
    
    # Check for suspicious values
    if voltage < 0 or voltage > 1000:
        issues.append(f"Voltage out of range: {voltage}V")
    
    if speed < 0 or speed > 10000:
        issues.append(f"Speed out of range: {speed} RPM")
    
    if temp < -50 or temp > 200:
        issues.append(f"Temperature out of range: {temp}°C")
    
    if all(val == 0 for val in [voltage, speed, temp]):
        issues.append("All values are zero - no CAN data?")
    
    if issues:
        print("   ⚠️  Issues found:")
        for issue in issues:
            print(f"      • {issue}")
    else:
        print("   ✓ Data appears valid")

def main():
    """Run all tests"""
    can = test_connection()
    if can is None:
        sys.exit(1)
    
    try:
        test_values(can)
        test_all_sensors(can)
        monitor_live(can, duration=5)
        test_data_validity(can)
        
        print()
        print("=" * 60)
        print("✓ All tests complete!")
        print("=" * 60)
        print()
        print("Next steps:")
        print("  • If values are changing: System is working! ✓")
        print("  • If all zeros: Check CAN bus connection")
        print("  • To start display: python3 ../display/qt_display.py")
        print()
        
    except KeyboardInterrupt:
        print("\n\n✓ Test interrupted by user")
    finally:
        can.close()

if __name__ == "__main__":
    main()