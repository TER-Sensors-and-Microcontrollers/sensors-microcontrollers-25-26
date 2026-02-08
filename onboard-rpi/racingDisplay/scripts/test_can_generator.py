#!/usr/bin/env python3
# test_can_generator.py
# Purpose: Generate realistic CAN bus test data for development/testing
# Simulates motor controller and BMS messages on vcan0
#
# Usage:
#   python3 test_can_generator.py
#   python3 test_can_generator.py --interface vcan0 --fast
#
# Abdi
# January 2026

import can
import time
import struct
import random
import argparse
import math
import sys


class CANTestGenerator:
    """
    Generates realistic CAN messages for testing the display system.
    Simulates vehicle behavior with realistic value ranges and variations.
    """
    
    def __init__(self, interface='vcan0', rate_hz=50):
        """
        Initialize test data generator.
        
        Args:
            interface: CAN interface name (e.g., 'vcan0', 'can0')
            rate_hz: Messages per second (default: 50 Hz)
        """
        self.interface = interface
        self.rate_hz = rate_hz
        self.running = True
        
        # Simulation state
        self.time_elapsed = 0
        self.pedal_position = 0.0
        self.motor_speed = 0
        self.direction = 1  # 1=forward, 0=reverse
        
        try:
            self.bus = can.interface.Bus(channel=interface, interface='virtual')
            print(f"✓ Connected to {interface}")
        except Exception as e:
            print(f"✗ Failed to connect to {interface}: {e}")
            print(f"  Make sure the interface exists: ip link show {interface}")
            sys.exit(1)
    
    def generate_motor_controller_messages(self):
        """Generate all motor controller CAN messages with realistic data"""
        
        # Simulate vehicle dynamics
        self.time_elapsed += 1.0 / self.rate_hz
        
        # Pedal oscillates 0-80% (simulating acceleration/deceleration)
        self.pedal_position = abs(math.sin(self.time_elapsed * 0.5)) * 0.8
        
        # Motor speed follows pedal with lag (0-3000 RPM)
        target_speed = self.pedal_position * 3000
        self.motor_speed += (target_speed - self.motor_speed) * 0.1  # Smooth
        
        # Motor temps increase with speed
        base_temp = 25.0  # Ambient
        motor_temp = base_temp + (self.motor_speed / 3000) * 60  # Up to 85°C
        coolant_temp = base_temp + (self.motor_speed / 3000) * 40  # Up to 65°C
        heatsink_temp = base_temp + (self.motor_speed / 3000) * 50  # Up to 75°C
        cb_temp = base_temp + (self.motor_speed / 3000) * 30  # Up to 55°C
        
        # Electrical values
        voltage = 96.0 + random.uniform(-2, 2)  # 94-98V
        current = (self.motor_speed / 3000) * 150  # 0-150A
        power = voltage * current
        torque = (current * 0.5)  # Simplified torque calculation
        
        # Motor angle (0-360 degrees)
        motor_angle = (self.time_elapsed * self.motor_speed / 60 * 360) % 360
        
        messages = []
        
        # ID 161 (0xA1): Control Board Temp
        data = struct.pack('<H', int(cb_temp * 10)) + bytes(6)
        messages.append(can.Message(arbitration_id=161, data=data, is_extended_id=False))
        
        # ID 162 (0xA2): Coolant, Heatsink, Motor Temps
        data = (struct.pack('<H', int(coolant_temp * 10)) +
                struct.pack('<H', int(heatsink_temp * 10)) +
                struct.pack('<H', int(motor_temp * 10)) + bytes(2))
        messages.append(can.Message(arbitration_id=162, data=data, is_extended_id=False))
        
        # ID 163 (0xA3): Pedal Positions (bit-packed)
        # Simplified: store pedal1 in first 2 bytes
        pedal1_raw = int(self.pedal_position * 1000)
        pedal2_raw = int(self.pedal_position * 100) + 103  # Slightly different
        data = struct.pack('<H', pedal1_raw) + struct.pack('<H', pedal2_raw) + bytes(4)
        messages.append(can.Message(arbitration_id=163, data=data, is_extended_id=False))
        
        # ID 165 (0xA5): Motor Angle & Speed
        data = (struct.pack('<H', int(motor_angle * 10)) +
                struct.pack('<H', int(self.motor_speed)) + bytes(4))
        messages.append(can.Message(arbitration_id=165, data=data, is_extended_id=False))
        
        # ID 166 (0xA6): DC Current
        data = bytes(6) + struct.pack('<H', int(current * 10))
        messages.append(can.Message(arbitration_id=166, data=data, is_extended_id=False))
        
        # ID 167 (0xA7): DC Voltage
        data = struct.pack('<H', int(voltage * 10)) + bytes(6)
        messages.append(can.Message(arbitration_id=167, data=data, is_extended_id=False))
        
        # ID 170 (0xAA): States & Direction
        vsm_state = 5  # Running state
        inv_state = 3  # Active state
        data = bytes([vsm_state, 0, inv_state, 0, 0, 0, 0, self.direction])
        messages.append(can.Message(arbitration_id=170, data=data, is_extended_id=False))
        
        # ID 172 (0xAC): Torque & Timer
        data = (struct.pack('<H', int(torque * 10)) +
                struct.pack('<H', int(self.time_elapsed * 1000) % 65536) + bytes(4))
        messages.append(can.Message(arbitration_id=172, data=data, is_extended_id=False))
        
        return messages
    
    def generate_bms_messages(self):
        """Generate BMS CAN messages (IDs < 160)"""
        messages = []
        
        # Generate a few BMS cell messages (IDs 0-10)
        for cell_id in range(5):
            # Cell voltages around 3.7V (typical Li-ion)
            voltage = int((3.7 + random.uniform(-0.1, 0.1)) * 1000)
            resistance = int(random.uniform(50, 150))  # mOhm
            
            # BMS message format (simplified)
            data = (bytes(2) +
                    struct.pack('<H', resistance) +
                    struct.pack('<H', voltage) + bytes(2))
            messages.append(can.Message(arbitration_id=cell_id, data=data, is_extended_id=False))
        
        return messages
    
    def send_messages(self, messages):
        """Send list of CAN messages"""
        for msg in messages:
            try:
                self.bus.send(msg)
            except can.CanError as e:
                print(f"✗ Failed to send message ID {msg.arbitration_id:03X}: {e}")
    
    def run(self):
        """Main loop: continuously generate and send test data"""
        print(f"✓ Generating test CAN data at {self.rate_hz} Hz")
        print(f"  Interface: {self.interface}")
        print(f"  Press Ctrl+C to stop")
        print()
        
        message_count = 0
        start_time = time.time()
        
        try:
            while self.running:
                loop_start = time.time()
                
                # Generate and send motor controller messages
                motor_msgs = self.generate_motor_controller_messages()
                self.send_messages(motor_msgs)
                
                # Generate and send BMS messages (less frequently)
                if message_count % 5 == 0:  # Every 5th iteration
                    bms_msgs = self.generate_bms_messages()
                    self.send_messages(bms_msgs)
                
                message_count += 1
                
                # Print status every second
                if message_count % self.rate_hz == 0:
                    elapsed = time.time() - start_time
                    actual_rate = message_count / elapsed
                    print(f"\rMessages: {message_count:5d}  |  "
                          f"Rate: {actual_rate:.1f} Hz  |  "
                          f"Speed: {self.motor_speed:.0f} RPM  |  "
                          f"Pedal: {self.pedal_position*100:.0f}%  |  "
                          f"Temp: {25 + (self.motor_speed/3000)*60:.1f}°C", end="")
                
                # Sleep to maintain desired rate
                sleep_time = (1.0 / self.rate_hz) - (time.time() - loop_start)
                if sleep_time > 0:
                    time.sleep(sleep_time)
                    
        except KeyboardInterrupt:
            print("\n\n✓ Stopped by user")
        finally:
            self.cleanup()
    
    def cleanup(self):
        """Clean up resources"""
        self.bus.shutdown()
        print("✓ CAN bus closed")


def main():
    """Entry point with argument parsing"""
    parser = argparse.ArgumentParser(
        description='Generate realistic CAN test data for electric racing display',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Default: vcan0 at 50 Hz
  %(prog)s --interface can0         # Use real CAN interface
  %(prog)s --rate 100 --fast        # 100 Hz, faster simulation
        """
    )
    
    parser.add_argument(
        '--interface', '-i',
        default='vcan0',
        help='CAN interface (default: vcan0)'
    )
    
    parser.add_argument(
        '--rate', '-r',
        type=int,
        default=50,
        help='Messages per second (default: 50 Hz)'
    )
    
    parser.add_argument(
        '--fast', '-f',
        action='store_true',
        help='Fast simulation (2x speed)'
    )
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("  CAN Test Data Generator")
    print("=" * 60)
    print()
    
    # Adjust rate if fast mode
    rate = args.rate * 2 if args.fast else args.rate
    
    # Create and run generator
    generator = CANTestGenerator(interface=args.interface, rate_hz=rate)
    generator.run()


if __name__ == "__main__":
    main()