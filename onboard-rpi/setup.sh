#!/bin/bash
# setup.sh
# Docker container startup script for Electric Racing Display System
# Sets up virtual CAN interface and optionally starts the display system

set -e  # Exit on error

echo "========================================"
echo "  Electric Racing Docker Container"
echo "========================================"
echo ""

# --- Virtual CAN Interface Setup ---
echo "🔧 Setting up VCAN interface vcan0..."

# Check if running with sufficient privileges
if ! sudo -n true 2>/dev/null; then
    echo "⚠️  Warning: Running without sudo privileges"
    echo "   Some CAN setup commands may fail"
fi

# Add the virtual CAN link (vcan0)
if ! ip link show vcan0 &>/dev/null; then
    sudo ip link add dev vcan0 type vcan 2>/dev/null || \
        echo "⚠️  Could not create vcan0 (may already exist or need --privileged)"
else
    echo "   • vcan0 already exists"
fi

# Set the link up
if sudo ip link set up vcan0 2>/dev/null; then
    echo "✓ VCAN interface vcan0 is ready"
else
    echo "⚠️  Could not bring up vcan0"
    echo "   Make sure Docker is run with: --privileged or --cap-add=NET_ADMIN"
fi

# Verify vcan0 is up
if ip link show vcan0 | grep -q "UP"; then
    echo "✓ vcan0 is UP and running"
else
    echo "⚠️  vcan0 may not be functioning correctly"
fi

echo ""
echo "========================================"
echo "  Available Commands:"
echo "========================================"
echo ""
echo "Testing:"
echo "  • candump vcan0              - Monitor CAN traffic"
echo "  • cansend vcan0 0A5#...      - Send test CAN message"
echo "  • python3 scripts/test_system.py  - Test the system"
echo ""
echo "System:"
echo "  • ./scripts/start_system.sh  - Start full system"
echo "  • python3 core/can_processor.py    - Start CAN processor only"
echo "  • python3 display/qt_display.py    - Start Qt display only"
echo ""
echo "Development:"
echo "  • qtcreator                  - Launch Qt Creator IDE"
echo "  • cd /workspace              - Go to project folder"
echo ""

# Check if racing-display project exists
if [ -d "/workspace/racing-display" ]; then
    cd /workspace/racing-display
    echo "✓ Found racing-display project"
    echo ""
    
    # Check for startup mode from environment variable
    case "${STARTUP_MODE:-shell}" in
        processor)
            echo "🚀 Starting CAN processor..."
            exec python3 core/can_processor.py
            ;;
        display)
            echo "🚀 Starting Qt display..."
            echo "   (Make sure X11 forwarding is enabled)"
            exec python3 display/qt_display.py
            ;;
        full)
            echo "🚀 Starting full system..."
            exec ./scripts/start_system.sh
            ;;
        test)
            echo "🧪 Running system tests..."
            python3 scripts/test_system.py
            exec /bin/bash
            ;;
        *)
            echo "📍 Current directory: $(pwd)"
            echo ""
            echo "💡 Tip: Set STARTUP_MODE environment variable:"
            echo "   processor - Auto-start CAN processor"
            echo "   display   - Auto-start Qt display"
            echo "   full      - Auto-start both"
            echo "   test      - Run tests then drop to shell"
            echo "   shell     - Just start a shell (default)"
            echo ""
            exec /bin/bash
            ;;
    esac
else
    echo "⚠️  Project not found at /workspace/racing-display"
    echo ""
    echo "📁 Expected directory structure:"
    echo "   /workspace/racing-display/"
    echo "   ├── config/"
    echo "   ├── core/"
    echo "   ├── display/"
    echo "   ├── scripts/"
    echo "   └── ..."
    echo ""
    echo "💡 Mount your project with:"
    echo "   docker run -v /path/to/racing-display:/workspace/racing-display ..."
    echo ""
    exec /bin/bash
fi