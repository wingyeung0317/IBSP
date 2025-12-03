# LoRa Health Monitoring System (IBSP)

Comprehensive IoT health monitoring system using ESP32, LoRa communication, and real-time dashboard.

## System Overview

This system monitors vital signs and detects falls using wearable ESP32 sensors, transmits data via LoRa to a Raspberry Pi gateway, and displays real-time information on a web dashboard.

### Key Features
- **Fall Detection**: Multi-stage algorithm using accelerometer and gyroscope
- **ECG Monitoring**: Real-time heart rate and ECG waveform analysis
- **Temperature Sensing**: Non-contact body temperature measurement
- **Noise Monitoring**: Environmental sound level tracking
- **LoRa Communication**: Long-range wireless transmission (1-2 km)
- **Real-time Dashboard**: Live data visualization with alerts
- **Data Persistence**: PostgreSQL database for historical analysis

## Hardware Requirements

### ESP32 Health Monitor
- Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
- MPU6050 (Accelerometer/Gyroscope)
- AD8232 (ECG Sensor)
- MLX90614 (IR Temperature Sensor)
- MAX4466 (Microphone Module)

### LoRa Gateway
- Vision Master E213 (ESP32-S3 + SX1262)
- Antenna (915 MHz or 868 MHz)

### Server Platform
- Raspberry Pi 4 (2GB+ RAM recommended)
- microSD Card (16GB+ recommended)
- Power supply (5V 3A)

## Quick Start

### 1. Program ESP32 Health Monitor

```bash
cd esp
pio run -t upload -t monitor
```

### 2. Program Vision Master E213 Gateway

```bash
cd vision-master
pio run -t upload -t monitor
```

### 3. Setup Raspberry Pi

```bash
# On Raspberry Pi
cd raspberry-pi
chmod +x setup.sh
./setup.sh

# Reboot to apply changes
sudo reboot

# After reboot, test receiver
python3 uart_lora_receiver.py
```

### 4. Access Dashboard

Open browser: `http://<raspberry-pi-ip>:5001`

## Detailed Documentation

- **[Raspberry Pi Setup](raspberry-pi/README.md)** - Complete Raspberry Pi configuration
- **[Vision Master Setup](raspberry-pi/VISION_MASTER_SETUP.md)** - LoRa gateway configuration
- **[Docker Commands](backend/DOCKER_COMMANDS.md)** - Backend management

## System Architecture

```
ESP32 Monitor → LoRa → Vision Master E213 → UART → Raspberry Pi → Docker Services → Dashboard
```

See main README for detailed architecture diagram.

## Configuration

### LoRa Parameters (must match on all devices)
- Frequency: 915.0 MHz (US/Asia) or 868.0 MHz (EU)
- Bandwidth: 125 kHz
- Spreading Factor: 9
- Coding Rate: 4/7
- TX Power: 22 dBm

### Packet Transmission
- Realtime data: Every 60 seconds
- ECG data: Every 306 seconds (~5 min)
- Fall events: Immediate

## Testing

```bash
# Test UART
python3 raspberry-pi/test_uart.py check

# Test LoRa reception
python3 raspberry-pi/uart_lora_receiver.py

# Test API
curl http://localhost:5000/health
```

## Troubleshooting

See [VISION_MASTER_SETUP.md](raspberry-pi/VISION_MASTER_SETUP.md) for comprehensive troubleshooting.

## License

MIT License (or your preferred license)

