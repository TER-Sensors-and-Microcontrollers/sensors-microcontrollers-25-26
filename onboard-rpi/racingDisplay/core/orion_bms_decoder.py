# orion_bms_decoder.py

class OrionBMSDecoder:
    """
    Decodes Orion BMS CAN messages into real values.
    """

    def __init__(self):
        self.data = {
            "pack_voltage": 0.0,
            "pack_current": 0.0,
            "soc": 0.0,
            "max_temp": 0.0
        }

    def decode(self, msg):
        """
        msg: python-can message
        """
        can_id = msg.arbitration_id
        d = msg.data

        # Pack Voltage
        if can_id == 0x6B0:
            raw = (d[0] << 8) | d[1]
            self.data["pack_voltage"] = raw * 0.1
            print(f"Pack Voltage: {self.data['pack_voltage']} V")

        # Pack Current
        elif can_id == 0x6B1:
            raw = (d[0] << 8) | d[1]
            self.data["pack_current"] = raw * 0.1
            print(f"Pack Current: {self.data['pack_current']} A")

        # State of Charge
        elif can_id == 0x6B2:
            self.data["soc"] = d[0] * 0.5
            print(f"SOC: {self.data['soc']} %")

        # Max Temperature
        elif can_id == 0x6B3:
            self.data["max_temp"] = d[0] - 40
            print(f"Max Temp: {self.data['max_temp']} °C")
        elif can_id ==0x36:
            print("Received CAN message with ID 0x36, but no decoding implemented yet.")

        return self.data