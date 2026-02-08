#!/bin/bash
# start_system.sh
# Purpose: Start all components of the electric racing display system
# 
# Usage:
#   ./start_system.sh              # Start all (defaults to can0)
#   ./start_system.sh vcan0        # Start in simulation mode
#   ./start_system.sh --no-display # Headless mode
#
# January 2026

# Get script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR" # Adjusted to project root if script is in root

# Default options
INTERFACE=${1:-can0}
START_DISPLAY=true

# Parse flags
for arg in "$@"; do
    if [ "$arg" == "--no-display" ]; then
        START_DISPLAY=false
    fi
done

# Handle cleanup on exit
cleanup() {
    echo ""
    echo "🛑 Shutting down system..."
    kill $CAN_PID $DISPLAY_PID $GEN_PID 2>/dev/null
    exit
}
trap cleanup SIGINT SIGTERM

echo "=========================================="
echo "  Electric Racing System Startup"
echo "=========================================="
echo "Interface:    $INTERFACE"
echo "Project Root: $PROJECT_ROOT"
echo "=========================================="

# 1. Ensure CAN interface is ready
if ! ip link show $INTERFACE &> /dev/null; then
    echo "⚠️  $INTERFACE not found. Attempting setup..."
    sudo "$PROJECT_ROOT/scripts/setup_can.sh" "$INTERFACE"
else
    if ! ip link show $INTERFACE | grep -q "UP"; then
        echo "⚠️  $INTERFACE is DOWN. Bringing it up..."
        sudo "$PROJECT_ROOT/scripts/setup_can.sh" "$INTERFACE"
    fi
fi

# 2. Start Simulation Generator (Only if using vcan0)
if [[ $INTERFACE == vcan* ]]; then
    echo "🧪 Simulation mode detected. Starting generator..."
    python3 "$PROJECT_ROOT/scripts/test_can_generator.py" --interface "$INTERFACE" &
    GEN_PID=$!
    echo "   Generator PID: $GEN_PID"
fi

# 3. Start CAN Processor (The Brain)
echo "🚀 Starting CAN processor..."
# Set PYTHONPATH so it finds config/ and core/
export PYTHONPATH="$PYTHONPATH:$PROJECT_ROOT/core:$PROJECT_ROOT/config"
python3 "$PROJECT_ROOT/core/can_processor.py" &
CAN_PID=$!
sleep 2 # Wait for shared memory initialization

if ! ps -p $CAN_PID > /dev/null; then
    echo "❌ CAN processor failed to start!"
    exit 1
fi

# 4. Start QML Display (The Face)
if [ "$START_DISPLAY" = true ]; then
    echo "🚀 Starting QML Display..."
    
    # Ensure DISPLAY is set for the RPi screen
    if [ -z "$DISPLAY" ]; then
        export DISPLAY=:0
    fi
    
    python3 "$PROJECT_ROOT/display/qml_display.py" &
    DISPLAY_PID=$!
    echo "   Display PID: $DISPLAY_PID"
else
    echo "ℹ️  Headless mode: Display skipped."
fi

echo ""
echo "✅ System is RUNNING"
echo "------------------------------------------"
echo "Press Ctrl+C to stop all components"
echo "------------------------------------------"

# Keep script alive to maintain the trap
wait