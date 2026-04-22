# orion_bms_decoder.py
# Decodes Orion BMS 2 CAN messages into engineering values.
#
# Active IDs confirmed on hardware (CAN1, 250 kbps):
#   0xC0 — Pack instantaneous voltage + State of Charge
#   0xC1 — Cell temperature (Low / High)
#   0xC2 — Fault codes
#
# NOTE on 0xC0 voltage scaling:
#   The teammate specified "inst.voltage × 0.1", but live data shows
#   bytes 0-1 LE = 59 536 which gives an impossible 5953 V.
#   Interpreting the same bytes as big-endian × 0.01 (10 mV/bit)
#   gives 37 096 × 0.01 = 370.96 V — correct for a ~370 V racing pack.
#   Verify against the Orion BMS 2 software display and adjust if needed.

import struct


class OrionBMSDecoder:
    """
    Accumulates decoded Orion BMS 2 values across successive messages.
    Each call to decode() updates only the fields relevant to that CAN ID
    and returns the full accumulated state dict.
    """

    def __init__(self):
        self.data = {
            "pack_voltage": 0.0,   # V
            "pack_current": 0.0,   # A  (not yet decoded — placeholder)
            "soc":          0.0,   # %  0.0–100.0
            "high_temp":    0.0,   # °C highest cell temperature
            "low_temp":     0.0,   # °C lowest cell temperature
            "fault_active": False, # True when 0xC2 byte 5 == 0x2A (42)
        }

    # ------------------------------------------------------------------
    def decode(self, msg):
        """
        Decode one Orion BMS 2 message and update accumulated state.

        Args:
            msg: python-can Message object

        Returns:
            dict: full accumulated data dict after update
        """
        can_id = msg.arbitration_id
        d      = msg.data

        if can_id == 0xC0:
            if len(d) < 7:
                return self.data

            # Pack voltage — big-endian uint16, 10 mV/bit (0.01 V/bit)
            # See note in module header about the 0.1 vs 0.01 discrepancy.
            raw_v = struct.unpack('>H', bytes(d[0:2]))[0]
            self.data["pack_voltage"] = raw_v * 0.01

            # State of Charge — byte 6, 0.5 % per bit, hard-clamped 0–100 %
            soc_raw = d[6]
            self.data["soc"] = min(100.0, max(0.0, soc_raw * 0.5))

        elif can_id == 0xC1:
            if len(d) < 2:
                return self.data

            # Temperatures use offset encoding:  celsius = raw_byte − 40
            # Range: byte 0 → −40 °C … 80 °C (raw 0 … 120)
            # Byte 0 = Low cell temperature  (coldest cell)
            # Byte 1 = High cell temperature (hottest cell)
            self.data["low_temp"]  = float(d[0]) - 40.0
            self.data["high_temp"] = float(d[1]) - 40.0

        elif can_id == 0xC2:
            if len(d) < 6:
                return self.data

            # Fault indicator: byte 5 must equal 0x2A (42) for fault active.
            # When this byte equals 42 the BMS has flagged a fault condition.
            print(f"DEBUG: ID 0xC2 | Byte 5 raw value: {d[5]} | Fault condition (== 42): {d[5] == 42}")
            self.data["fault_active"] = (d[5] == 0x2A)

        return self.data