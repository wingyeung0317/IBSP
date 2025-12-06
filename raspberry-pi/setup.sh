#!/bin/bash
# Raspberry Pi Setup Script for LoRa Health Monitor System
# =========================================================
# This script sets up the complete system on Raspberry Pi

set -e  # Exit on error

echo "=================================================="
echo "Raspberry Pi LoRa Health Monitor Setup"
echo "=================================================="
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo "❌ Please do not run as root (no sudo)"
   exit 1
fi

# Update system
echo "[1/8] Updating system packages..."
sudo apt-get update
sudo apt-get upgrade -y

# Install dependencies
echo "[2/8] Installing dependencies..."
sudo apt-get install -y \
    python3 \
    python3-pip \
    docker.io \
    docker-compose \
    git \
    minicom \
    screen

# Install Python packages
echo "[3/8] Installing Python packages..."
pip3 install --user pyserial requests --break-system-packages

# Install Python packages for root (required for systemd service)
echo "Installing Python packages for system (root)..."
sudo pip3 install pyserial requests --break-system-packages

# Configure UART
echo "[4/8] Configuring UART..."
sudo raspi-config nonint do_serial 1  # Disable serial console
sudo raspi-config nonint set_config_var enable_uart 1 /boot/config.txt

# Add user to dialout group for UART access
echo "[5/8] Setting permissions..."
sudo usermod -a -G dialout $USER
sudo usermod -a -G tty $USER
sudo usermod -a -G docker $USER

# Create project directory
echo "[6/8] Creating project directory..."
mkdir -p ~/IBSP
cd ~/IBSP

# Clone repository (if not already present)
if [ ! -d ".git" ]; then
    echo "Repository not found. Please clone manually:"
    echo "  git clone <your-repo-url> ~/IBSP"
    echo "Or copy files manually to ~/IBSP/"
fi

# Make scripts executable
if [ -f "raspberry-pi/uart_lora_receiver.py" ]; then
    chmod +x raspberry-pi/uart_lora_receiver.py
    echo "✅ Made receiver script executable"
fi

# Install systemd service
echo "[7/8] Installing systemd service..."
sudo tee /etc/systemd/system/lora-receiver.service > /dev/null <<EOF
[Unit]
Description=LoRa UART Receiver for Health Monitor
After=network.target docker.service

[Service]
Type=simple
User=root
WorkingDirectory=$HOME/IBSP/raspberry-pi
ExecStart=/usr/bin/python3 $HOME/IBSP/raspberry-pi/uart_lora_receiver.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd
sudo systemctl daemon-reload

# Docker setup
echo "[8/8] Setting up Docker containers..."
if [ -d "backend" ]; then
    cd backend
    
    # Start Docker containers
    echo "Starting Docker containers..."
    docker-compose up -d
    
    # Wait for containers to be healthy
    echo "Waiting for containers to start..."
    sleep 10
    
    # Check container status
    docker-compose ps
    
    cd ..
else
    echo "⚠️  Backend directory not found, skipping Docker setup"
fi

echo ""
echo "=================================================="
echo "✅ Setup Complete!"
echo "=================================================="
echo ""
echo "Next steps:"
echo ""
echo "1. Reboot to apply UART changes:"
echo "   sudo reboot"
echo ""
echo "2. After reboot, verify UART:"
echo "   ls -l /dev/ttyS0"
echo ""
echo "3. Test receiver script:"
echo "   cd ~/IBSP/raspberry-pi"
echo "   python3 uart_lora_receiver.py"
echo ""
echo "4. Enable auto-start:"
echo "   sudo systemctl enable lora-receiver"
echo "   sudo systemctl start lora-receiver"
echo ""
echo "5. Check logs:"
echo "   sudo journalctl -u lora-receiver -f"
echo ""
echo "6. Access dashboard:"
echo "   http://$(hostname -I | awk '{print $1}'):5001"
echo ""
echo "=================================================="
