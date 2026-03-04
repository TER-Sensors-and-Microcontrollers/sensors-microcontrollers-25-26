#!/bin/bash
# start_dual_display.sh
# Purpose: Launch CAN processor and QML display in separate terminals

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

echo "=========================================="
echo "  Starting Racing Display System"
echo "=========================================="
echo ""

# Check if running in a graphical environment
if [ -z "$DISPLAY" ]; then
    echo "❌ Error: No display detected. Run this script in a graphical environment."
    exit 1
fi

# Determine which terminal emulator is available
if command -v gnome-terminal &> /dev/null; then
    TERMINAL="gnome-terminal --"
elif command -v xterm &> /dev/null; then
    TERMINAL="xterm -hold -e"
elif command -v konsole &> /dev/null; then
    TERMINAL="konsole -e"
else
    echo "❌ Error: No terminal emulator found (gnome-terminal, xterm, or konsole)"
    exit 1
fi

echo "🚀 Launching CAN Processor terminal..."
$TERMINAL bash -c "
    cd '$PROJECT_ROOT' || exit
    echo '=========================================='
    echo '  CAN Processor Terminal'
    echo '=========================================='
    echo ''
    echo '🧹 Cleaning shared memory...'
    sudo rm /dev/shm/mem123 2>/dev/null
    echo '✓ Shared memory cleaned'
    echo ''
    echo '🔧 Activating virtual environment...'
    source venv/bin/activate
    echo '✓ Virtual environment activated'
    echo ''
    echo '🚀 Starting CAN processor...'
    python3 core/can_processor.py
    echo ''
    echo '⚠️  CAN Processor stopped. Press Enter to close...'
    read
" &

echo "⏳ Waiting 2 seconds for shared memory setup..."
sleep 2

echo "🚀 Launching QML Display terminal..."
$TERMINAL bash -c "
    cd '$PROJECT_ROOT/display' || exit
    echo '=========================================='
    echo '  QML Display Terminal'
    echo '=========================================='
    echo ''
    echo '🚀 Starting QML display...'
    python3 qml_display.py
    echo ''
    echo '⚠️  QML Display stopped. Press Enter to close...'
    read
" &

echo ""
echo "✓ Both terminals launched successfully"
echo ""
echo "To stop the system:"
echo "  • Press Ctrl+C in each terminal window"
echo "  • Or close the terminal windows"
echo ""