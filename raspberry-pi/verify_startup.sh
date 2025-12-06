#!/bin/bash
# Verify System Startup Configuration
# =====================================

echo "=================================================="
echo "System Startup Verification"
echo "=================================================="
echo ""

echo "[1] Serial Console Configuration"
echo "-----------------------------------"
if [ -f /boot/firmware/cmdline.txt ]; then
    if grep -q "console=serial0" /boot/firmware/cmdline.txt; then
        echo "❌ Serial console is ENABLED in /boot/firmware/cmdline.txt"
        echo "   This will cause conflicts with /dev/ttyS0"
    else
        echo "✅ Serial console is DISABLED in /boot/firmware/cmdline.txt"
    fi
elif [ -f /boot/cmdline.txt ]; then
    if grep -q "console=serial0" /boot/cmdline.txt; then
        echo "❌ Serial console is ENABLED in /boot/cmdline.txt"
    else
        echo "✅ Serial console is DISABLED in /boot/cmdline.txt"
    fi
fi
echo ""

echo "[2] Getty Service on ttyS0"
echo "-----------------------------------"
if systemctl is-enabled serial-getty@ttyS0.service 2>/dev/null | grep -q "enabled"; then
    echo "❌ Getty service is ENABLED on ttyS0"
    echo "   Run: sudo systemctl disable serial-getty@ttyS0.service"
else
    echo "✅ Getty service is DISABLED on ttyS0"
fi

if systemctl is-active serial-getty@ttyS0.service 2>/dev/null | grep -q "active"; then
    echo "❌ Getty service is RUNNING on ttyS0"
    echo "   Run: sudo systemctl stop serial-getty@ttyS0.service"
else
    echo "✅ Getty service is NOT RUNNING on ttyS0"
fi
echo ""

echo "[3] UART Device Permissions"
echo "-----------------------------------"
if [ -e /dev/ttyS0 ]; then
    ls -l /dev/ttyS0
    if ps aux | grep -v grep | grep "agetty.*ttyS0" > /dev/null; then
        echo "❌ agetty process is using ttyS0"
        ps aux | grep -v grep | grep "agetty.*ttyS0"
    else
        echo "✅ No agetty process on ttyS0"
    fi
else
    echo "❌ /dev/ttyS0 not found"
fi
echo ""

echo "[4] Docker Service"
echo "-----------------------------------"
systemctl is-enabled docker.service
systemctl is-active docker.service
echo ""

echo "[5] Docker Compose Service"
echo "-----------------------------------"
systemctl is-enabled docker-compose-app.service
systemctl is-active docker-compose-app.service
echo ""

echo "[6] LoRa Receiver Service"
echo "-----------------------------------"
systemctl is-enabled lora-receiver.service
systemctl is-active lora-receiver.service
echo ""

echo "[7] Service Startup Order"
echo "-----------------------------------"
echo "Checking dependencies..."
systemctl show -p After lora-receiver.service | grep After
echo ""

echo "[8] Docker Containers"
echo "-----------------------------------"
cd /home/wing/IBSP/backend 2>/dev/null && docker-compose ps || echo "❌ Cannot access backend directory"
echo ""

echo "[9] Recent LoRa Packets"
echo "-----------------------------------"
sudo journalctl -u lora-receiver.service -n 10 --no-pager | grep -E "Packet #|Type|✅|❌" | tail -5
echo ""

echo "=================================================="
echo "Verification Complete"
echo "=================================================="
