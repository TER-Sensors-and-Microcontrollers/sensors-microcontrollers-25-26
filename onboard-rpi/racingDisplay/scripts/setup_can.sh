#!/bin/bash
# setup_can.sh
# Electric Racing CAN Setup Script
# Production version for USB-CAN @ 250000 bitrate

INTERFACE=${1:-can0}
BITRATE=${2:-250000}

echo "=========================================="
echo "  Electric Racing CAN Setup"
echo "=========================================="
echo "Interface: $INTERFACE"
echo "Bitrate:   $BITRATE bps"
echo ""

# Ensure script is run as root (systemd will handle this)
if [ "$EUID" -ne 0 ]; then
    echo "❌ Must be run as root"
    exit 1
fi

# Wait briefly for USB-CAN to initialize
sleep 2

# Verify interface exists
if ! ip link show "$INTERFACE" &> /dev/null; then
    echo "❌ Error: $INTERFACE not found. Is USB-CAN plugged in?"
    exit 1
fi

echo "🔧 Configuring $INTERFACE..."

# Bring down before configuration
ip link set "$INTERFACE" down 2>/dev/null

# Set bitrate
if ! ip link set "$INTERFACE" type can bitrate "$BITRATE"; then
    echo "❌ Failed to set bitrate"
    exit 1
fi

# Bring interface up
if ip link set "$INTERFACE" up; then
    echo "✓ $INTERFACE is UP"
else
    echo "❌ Failed to bring up $INTERFACE"
    exit 1
fi

echo ""
echo "✓ CAN setup complete"
echo ""

ip -details link show "$INTERFACE"
