#!/bin/bash
# setup_can.sh
# Purpose: Configure physical CAN (can0) or virtual CAN (vcan0) on Raspberry Pi
# 
# Usage:
#   sudo ./setup_can.sh            # Defaults to can0 500k
#   sudo ./setup_can.sh vcan0      # Sets up virtual CAN for simulation
#
# January 2026

# Default values
INTERFACE=${1:-can0}
BITRATE=${2:-500000}

echo "=========================================="
echo "  Electric Racing CAN Setup"
echo "=========================================="
echo "Interface: $INTERFACE"
if [[ $INTERFACE != vcan* ]]; then
    echo "Bitrate:   $BITRATE bps"
fi
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "❌ Error: This script must be run as root (use sudo)"
    exit 1
fi

# Check if iproute2 is installed
if ! command -v ip &> /dev/null; then
    echo "❌ Error: iproute2 not installed"
    exit 1
fi

# Handle Virtual CAN vs Physical CAN
if [[ $INTERFACE == vcan* ]]; then
    echo "🌐 Setting up VIRTUAL interface: $INTERFACE"
    modprobe vcan
    ip link add dev $INTERFACE type vcan 2>/dev/null || echo "   Note: $INTERFACE already exists"
else
    echo "🔧 Setting up PHYSICAL interface: $INTERFACE"
    # Bring interface down to apply bitrate changes
    ip link set $INTERFACE down 2>/dev/null
    if ! ip link set $INTERFACE type can bitrate $BITRATE; then
        echo "❌ Error: Failed to configure hardware. Is the CAN hat connected?"
        exit 1
    fi
fi

# Bring interface up
echo "🔧 Bringing up $INTERFACE..."
if ip link set up $INTERFACE; then
    echo "✓ $INTERFACE is UP"
else
    echo "❌ Error: Failed to bring up $INTERFACE"
    exit 1
fi

# Final Verification
if ip link show $INTERFACE | grep -q "UP"; then
    echo ""
    echo "=========================================="
    echo "✓ Success! $INTERFACE is ready."
    echo "=========================================="
    echo ""
    echo "To monitor traffic, run:"
    echo "  candump $INTERFACE"
    echo ""
else
    echo "❌ Error: Interface is not UP"
    exit 1
fi

# Show interface details
ip -details link show $INTERFACE