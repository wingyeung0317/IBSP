# Quick Reference Card - LoRa Health Monitor

## System Overview
```
ESP32 (Sensors) → LoRa → Vision Master E213 → UART → Raspberry Pi → Docker → Dashboard
```

## LoRa Configuration (MUST MATCH ALL DEVICES!)

| Parameter | Value |
|-----------|-------|
| Frequency | 915.0 MHz (US/Asia) or 868.0 MHz (EU) |
| Bandwidth | 125 kHz |
| Spreading Factor | 9 |
| Coding Rate | 4/7 |
| Sync Word | 0x12 |
| TX Power | 22 dBm |
| Preamble Length | 8 |

## Pin Connections

### ESP32 (Heltec WiFi LoRa 32 V3)
- SX1262 NSS: GPIO 8
- SX1262 DIO1: GPIO 14
- SX1262 RESET: GPIO 12
- SX1262 BUSY: GPIO 13
- SPI: SCK=9, MISO=11, MOSI=10

### Vision Master E213
- SX1262 NSS: GPIO 10
- SX1262 DIO1: GPIO 5
- SX1262 RESET: GPIO 3
- SX1262 BUSY: GPIO 36

### Vision Master → Raspberry Pi UART
```
Vision Master E213    Raspberry Pi
TX (GPIO 1)      →    RX (GPIO 15, Pin 10)
RX (GPIO 2)      ←    TX (GPIO 14, Pin 8)
GND              →    GND (Pin 6)
VCC 5V           →    5V (Pin 2)
```

## Quick Commands

### ESP32
```bash
cd esp
pio run -t upload -t monitor
```

### Vision Master
```bash
cd vision-master
pio run -t upload -t monitor
```

### Raspberry Pi Setup
```bash
cd raspberry-pi
chmod +x setup.sh
./setup.sh
sudo reboot
```

### Start/Stop Services
```bash
# Start receiver
sudo systemctl start lora-receiver

# Stop receiver
sudo systemctl stop lora-receiver

# View logs
sudo journalctl -u lora-receiver -f

# Check Docker
docker ps
docker-compose up -d
```

### Testing
```bash
# Test UART
python3 test_uart.py check

# Test receiver
python3 uart_lora_receiver.py

# Test API
curl http://localhost:5000/health
```

## Packet Types

| Type | Name | Size | Interval |
|------|------|------|----------|
| 1 | Realtime | 10 bytes | 60s |
| 2 | ECG | 65 bytes | 306s |
| 3 | Fall Event | 45 bytes | Immediate |

## Troubleshooting Checklist

### ❌ No LoRa transmission
- [ ] Antenna connected?
- [ ] LoRa initialized OK? (check serial)
- [ ] Correct frequency (915/868)?
- [ ] SPI pins correct?

### ❌ No LoRa reception
- [ ] Parameters match ESP32?
- [ ] Sync word 0x12?
- [ ] Distance < 10m for testing?
- [ ] Antenna connected?

### ❌ No UART data
- [ ] UART enabled (raspi-config)?
- [ ] TX → RX crossed correctly?
- [ ] Baud rate 115200?
- [ ] Permissions OK (`ls -l /dev/ttyS0`)?

### ❌ No data on dashboard
- [ ] Docker running (`docker ps`)?
- [ ] API reachable (`curl localhost:5000/health`)?
- [ ] Receiver running (`systemctl status lora-receiver`)?
- [ ] Check logs (`journalctl -u lora-receiver -f`)?

## Useful Logs

```bash
# All logs
sudo journalctl -u lora-receiver -f &
docker logs health-monitor-api -f

# Last 100 lines
sudo journalctl -u lora-receiver -n 100

# API errors only
docker logs health-monitor-api 2>&1 | grep ERROR
```

## Web Interfaces

- Dashboard: `http://<raspberry-pi-ip>:5001`
- API: `http://<raspberry-pi-ip>:5000`
- API Health: `http://<raspberry-pi-ip>:5000/health`

## Expected Performance

- **Range:** 1-2 km (line of sight)
- **Battery Life:** 24-48 hours
- **Packet Success Rate:** >95%
- **Data Rate:** ~5.5 kbps
- **Latency:** 1-5 seconds

## Files Location

```
~/IBSP/
├── esp/                        ESP32 firmware
├── vision-master/              Vision Master firmware
├── raspberry-pi/               Raspberry Pi scripts
│   ├── uart_lora_receiver.py  Main receiver
│   ├── test_uart.py           UART tester
│   └── setup.sh               Setup script
└── backend/                    Docker services
    └── docker-compose.yml
```

## Emergency Reset

```bash
# Reset receiver
sudo systemctl restart lora-receiver

# Reset Docker
cd ~/IBSP/backend
docker-compose restart

# Reset everything
sudo systemctl restart lora-receiver
docker-compose restart
```

## Support Documents

- `raspberry-pi/README.md` - Raspberry Pi guide
- `raspberry-pi/VISION_MASTER_SETUP.md` - Detailed setup
- `MIGRATION_TO_LORA.md` - Migration guide
- `backend/DOCKER_COMMANDS.md` - Docker reference

## Quick Health Check

```bash
# 1. Check ESP32 (should see LoRa transmission)
pio device monitor

# 2. Check Vision Master (should see reception)
pio device monitor

# 3. Check Raspberry Pi (should see packets)
sudo journalctl -u lora-receiver -n 20

# 4. Check API (should return status)
curl http://localhost:5000/health

# 5. Check Docker (should see 3 containers)
docker ps
```

---

**Emergency Contact:** [Your contact info]
**Last Updated:** 2025-12-04
