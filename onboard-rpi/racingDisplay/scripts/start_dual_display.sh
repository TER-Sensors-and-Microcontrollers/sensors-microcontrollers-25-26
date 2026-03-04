#!/bin/bash
# start_dual_display.sh
# Purpose: Launch CAN processor and QML display in separate terminals

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

echo "=========================================="
echo "  Starting Racing Display System"
echo "=========================================="
echo ""

echo "🚀 Launching CAN Processor terminal..."
lxterminal -e bash -c "
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
lxterminal -e bash -c "
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