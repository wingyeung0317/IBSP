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

# Disable serial console in boot parameters
echo "Disabling serial console in boot config..."
if [ -f /boot/firmware/cmdline.txt ]; then
    sudo cp /boot/firmware/cmdline.txt /boot/firmware/cmdline.txt.bak
    sudo sed -i 's/console=serial0,[0-9]* //g' /boot/firmware/cmdline.txt
    echo "✅ Removed serial console from /boot/firmware/cmdline.txt"
elif [ -f /boot/cmdline.txt ]; then
    sudo cp /boot/cmdline.txt /boot/cmdline.txt.bak
    sudo sed -i 's/console=serial0,[0-9]* //g' /boot/cmdline.txt
    echo "✅ Removed serial console from /boot/cmdline.txt"
fi

# Disable getty on ttyS0
echo "Disabling getty service on ttyS0..."
sudo systemctl stop serial-getty@ttyS0.service 2>/dev/null || true
sudo systemctl disable serial-getty@ttyS0.service 2>/dev/null || true
sudo systemctl mask serial-getty@ttyS0.service 2>/dev/null || true
echo "✅ Getty service disabled on ttyS0"

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
After=network.target docker.service docker-compose-app.service

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

# Enable Docker to start on boot
echo "Enabling Docker service..."
sudo systemctl enable docker
sudo systemctl start docker

if [ -d "backend" ]; then
    cd backend
    
    # Start Docker containers
    echo "Starting Docker containers..."
    docker-compose up -d
    
    # Enable Docker Compose to start on boot (create systemd service)
    echo "Creating Docker Compose systemd service..."
    sudo tee /etc/systemd/system/docker-compose-app.service > /dev/null <<DOCKEREOF
[Unit]
Description=Docker Compose Application Service
Requires=docker.service
After=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
WorkingDirectory=$HOME/IBSP/backend
ExecStart=/usr/bin/docker-compose up -d
ExecStop=/usr/bin/docker-compose down
TimeoutStartSec=0

[Install]
WantedBy=multi-user.target
DOCKEREOF

    sudo systemctl daemon-reload
    sudo systemctl enable docker-compose-app.service
    echo "✅ Docker Compose will start automatically on boot"
    
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
