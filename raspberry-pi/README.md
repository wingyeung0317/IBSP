# LoRa Health Monitoring System - Raspberry Pi Gateway

This directory contains the Raspberry Pi gateway components for receiving LoRa packets from the ESP32 health monitor via Vision Master E213.

## System Architecture

```
ESP32 Health Monitor
        |
        | (LoRa 915 MHz)
        |
        v
Vision Master E213 Receiver
        |
        | (UART)
        |
        v
Raspberry Pi
        |
        | (HTTP POST)
        |
        v
Docker: Node.js Backend + PostgreSQL + Dashboard
```

## Files

- `uart_lora_receiver.py` - Python script to receive LoRa packets via UART
- `setup.sh` - Automated setup script for Raspberry Pi
- `.env.example` - Environment configuration template
- `VISION_MASTER_SETUP.md` - Detailed configuration guide
- `test_uart.py` - UART testing utility
- `requirements.txt` - Python dependencies

## Quick Start

### 1. Hardware Setup

Connect Vision Master E213 to Raspberry Pi:
```
Vision Master E213    →    Raspberry Pi
TX (GPIO 1)           →    RX (GPIO 15, Pin 10)
RX (GPIO 2)           →    TX (GPIO 14, Pin 8)
GND                   →    GND (Pin 6)
VCC 5V                →    5V (Pin 2)
```

### 2. Software Setup

```bash
# Clone repository to Raspberry Pi
cd ~
git clone <your-repo-url> IBSP
cd IBSP/raspberry-pi

# Configure environment variables
cp .env.example .env
nano .env  # Edit and set your SERVER_IP

# Run setup script
chmod +x setup.sh
./setup.sh

# Reboot to apply UART changes
sudo reboot
```

### 3. Test Reception

```bash
# Manual test (load environment variables first)
cd ~/IBSP/raspberry-pi
source .env
python3 uart_lora_receiver.py

# You should see output like:
# 🎧 Listening for LoRa packets...
# 📦 Packet #1
#    Device: ESP32-001
#    Type: Realtime (Port 1)
#    ...
```

### 4. Enable Auto-Start

```bash
# Enable service to start on boot
sudo systemctl enable lora-receiver
sudo systemctl start lora-receiver

# Check status
sudo systemctl status lora-receiver

# View logs
sudo journalctl -u lora-receiver -f
```

## Testing

### Test UART Loopback

```bash
# Connect TX to RX on Raspberry Pi
# Then run:
echo "test" > /dev/ttyS0 &
cat /dev/ttyS0
# Should see "test" output
```

### Test Vision Master Reception

```bash
# Monitor Vision Master output
screen /dev/ttyS0 115200

# Or use minicom
sudo minicom -D /dev/ttyS0 -b 115200
```

### Test Server Connection

```bash
# Check if Docker containers are running
docker ps

# Test API endpoint
curl http://localhost:5000/health

# Should return: {"status":"ok","timestamp":"..."}
```

## Monitoring

### View Receiver Logs
```bash
sudo journalctl -u lora-receiver -f
```

### View Docker Logs
```bash
docker logs health-monitor-api -f
```

### View Combined Logs
```bash
# Terminal 1
sudo journalctl -u lora-receiver -f

# Terminal 2
docker logs health-monitor-api -f
```

## Troubleshooting

### No UART Data

1. Check UART enabled:
   ```bash
   sudo raspi-config
   # Interface Options → Serial Port
   # Login shell: No
   # Serial hardware: Yes
   ```

2. Check permissions:
   ```bash
   ls -l /dev/ttyS0
   sudo chmod 666 /dev/ttyS0
   groups  # Should include 'dialout'
   ```

3. Check wiring:
   ```bash
   # TX should go to RX, RX to TX
   # GND must be connected
   ```

### No LoRa Packets

1. Check Vision Master E213:
   - Is it powered on?
   - Is antenna connected?
   - Are LoRa parameters matching ESP32?

2. Check ESP32:
   - Is it transmitting? (check ESP32 serial output)
   - Is antenna connected?
   - Check frequency matches (915 MHz)

3. Test range:
   - Start with < 10 meters
   - Clear line of sight
   - Move antennas apart from metal objects

### Server Not Responding

1. Check Docker:
   ```bash
   docker ps
   docker-compose up -d
   ```

2. Check network:
   ```bash
   curl http://localhost:5000/health
   ```

3. Restart services:
   ```bash
   cd ~/IBSP/backend
   docker-compose restart
   ```

## Performance

### Expected Packet Rate

- Realtime data: 1 packet per 60 seconds
- ECG data: 1 packet per 306 seconds (~5 minutes)
- Fall events: Immediate (on detection)

### UART Specifications

- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

### LoRa Range

With SF9 configuration:
- Indoor: 100-300 meters
- Outdoor (line of sight): 1-2 kilometers
- Urban: 500-1000 meters

## System Resources

### CPU Usage
- Receiver script: < 5%
- Docker containers: 10-20%
- Total: < 30%

### Memory Usage
- Receiver script: ~20 MB
- Docker containers: ~500 MB
- Recommended: Raspberry Pi 4 with 2GB+ RAM

### Storage
- Docker images: ~1 GB
- Database: Grows with data (estimate 1 MB per day)
- Logs: Rotate automatically

## Maintenance

### Update System

```bash
cd ~/IBSP
git pull
cd backend
docker-compose down
docker-compose build
docker-compose up -d
```

### Clear Database

```bash
cd ~/IBSP/backend
docker-compose down -v  # WARNING: Deletes all data
docker-compose up -d
```

### Backup Database

```bash
docker exec health-monitor-db pg_dump -U healthmonitor health_monitoring > backup.sql
```

### Restore Database

```bash
cat backup.sql | docker exec -i health-monitor-db psql -U healthmonitor -d health_monitoring
```

## Systemd Service Commands

```bash
# Start service
sudo systemctl start lora-receiver

# Stop service
sudo systemctl stop lora-receiver

# Restart service
sudo systemctl restart lora-receiver

# Enable auto-start
sudo systemctl enable lora-receiver

# Disable auto-start
sudo systemctl disable lora-receiver

# Check status
sudo systemctl status lora-receiver

# View logs
sudo journalctl -u lora-receiver -f

# View last 100 lines
sudo journalctl -u lora-receiver -n 100
```

## Support

For issues or questions:
1. Check logs: `sudo journalctl -u lora-receiver -f`
2. Check Docker: `docker logs health-monitor-api -f`
3. Verify wiring and configuration
4. See `VISION_MASTER_SETUP.md` for detailed troubleshooting

## License

[Your License Here]
