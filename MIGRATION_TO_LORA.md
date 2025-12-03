# Migration from WiFi to LoRa Communication

## Summary of Changes

This document outlines the changes made to convert the health monitoring system from WiFi to LoRa communication.

## System Changes

### Before (WiFi Architecture)
```
ESP32 → WiFi → Internet → Cloud Server → Database → Dashboard
```

### After (LoRa Architecture)
```
ESP32 → LoRa → Vision Master E213 → UART → Raspberry Pi → Docker → Dashboard
```

## Modified Files

### 1. ESP32 Firmware (`esp/src/main.cpp`)

**Changes:**
- Removed WiFi libraries and code
- Added RadioLib library for SX1262 LoRa module
- Replaced `WiFiComm` class with `LoRaComm` class
- Updated pin definitions for Heltec WiFi LoRa 32 V3
- Changed packet format to binary (removed Base64 encoding)

**Key Differences:**
```cpp
// OLD (WiFi)
#include <WiFi.h>
#include <HTTPClient.h>
WiFiComm wifiComm;

// NEW (LoRa)
#include <RadioLib.h>
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);
LoRaComm loraComm;
```

**LoRa Configuration:**
- Frequency: 915 MHz (configurable for region)
- Spreading Factor: 9
- Bandwidth: 125 kHz
- TX Power: 22 dBm
- Range: 1-2 km line of sight

### 2. PlatformIO Configuration (`esp/platformio.ini`)

**Added:**
```ini
lib_deps = 
    jgromes/RadioLib@^6.4.0
```

### 3. Backend Server (`backend/api/server.js`)

**No changes required!** The server already accepts binary data in the same packet format.

The packet structure remains identical:
- Packet Type 1 (Realtime): 10 bytes
- Packet Type 2 (ECG): 65 bytes
- Packet Type 3 (Fall Event): 45 bytes

## New Files Created

### Vision Master E213 Receiver

**`vision-master/src/main.cpp`**
- LoRa receiver firmware for Vision Master E213
- Receives packets from ESP32
- Forwards to Raspberry Pi via UART
- Displays basic info on built-in screen

**`vision-master/platformio.ini`**
- PlatformIO configuration for Vision Master E213
- Uses ESP32-S3 platform

### Raspberry Pi Gateway

**`raspberry-pi/uart_lora_receiver.py`**
- Python script to receive UART data from Vision Master
- Parses LoRa packets
- Forwards to backend API via HTTP POST
- Includes statistics and error handling

**`raspberry-pi/setup.sh`**
- Automated setup script for Raspberry Pi
- Installs dependencies
- Configures UART
- Sets up systemd service

**`raspberry-pi/test_uart.py`**
- UART testing utility
- Loopback test
- Receive test
- Availability check

**`raspberry-pi/requirements.txt`**
- Python dependencies (pyserial, requests)

**`raspberry-pi/README.md`**
- Complete Raspberry Pi setup guide
- Troubleshooting steps
- Monitoring commands

**`raspberry-pi/VISION_MASTER_SETUP.md`**
- Detailed Vision Master E213 configuration
- Hardware wiring diagrams
- LoRa parameter tuning
- Systemd service setup

## Migration Steps

### Step 1: Update ESP32 Firmware

1. Open ESP32 project in PlatformIO
2. The code has already been updated for LoRa
3. Configure frequency for your region (915 MHz US/Asia, 868 MHz EU)
4. Upload to ESP32:
   ```bash
   cd esp
   pio run -t upload
   ```

### Step 2: Program Vision Master E213

1. Open Vision Master project:
   ```bash
   cd vision-master
   ```
2. Verify pin definitions match your Vision Master model
3. Upload firmware:
   ```bash
   pio run -t upload
   ```

### Step 3: Setup Raspberry Pi

1. Copy project to Raspberry Pi
2. Run setup script:
   ```bash
   cd raspberry-pi
   chmod +x setup.sh
   ./setup.sh
   ```
3. Reboot:
   ```bash
   sudo reboot
   ```

### Step 4: Wire Vision Master to Raspberry Pi

```
Vision Master E213    →    Raspberry Pi
TX (GPIO 1)           →    RX (GPIO 15, Pin 10)
RX (GPIO 2)           →    TX (GPIO 14, Pin 8)
GND                   →    GND (Pin 6)
VCC 5V                →    5V (Pin 2)
```

### Step 5: Start Services

```bash
# Enable auto-start
sudo systemctl enable lora-receiver
sudo systemctl start lora-receiver

# Check status
sudo systemctl status lora-receiver

# View logs
sudo journalctl -u lora-receiver -f
```

### Step 6: Start Docker Backend

```bash
cd backend
docker-compose up -d
```

### Step 7: Test System

1. Power on ESP32
2. Check ESP32 serial output for LoRa transmission
3. Check Vision Master serial output for reception
4. Check Raspberry Pi logs for UART data
5. Access dashboard: `http://<raspberry-pi-ip>:5001`

## Advantages of LoRa Over WiFi

### 1. Range
- **WiFi:** 50-100 meters
- **LoRa:** 1-2 km (line of sight)

### 2. Power Consumption
- **WiFi:** High power (constant connection)
- **LoRa:** Low power (transmit only when needed)

### 3. Battery Life
- **WiFi:** 4-8 hours
- **LoRa:** 24-48 hours (with same battery)

### 4. Infrastructure
- **WiFi:** Requires WiFi network everywhere
- **LoRa:** Direct point-to-point, no infrastructure needed

### 5. Penetration
- **WiFi:** Poor through walls/obstacles
- **LoRa:** Better penetration, works in rural/outdoor areas

## Disadvantages of LoRa

### 1. Data Rate
- **WiFi:** Mbps (very fast)
- **LoRa:** ~5 kbps (slow but sufficient for sensors)

### 2. Latency
- **WiFi:** < 1 second
- **LoRa:** 1-5 seconds (depends on SF)

### 3. Complexity
- **WiFi:** Simple (built-in to ESP32)
- **LoRa:** Requires separate module and gateway

### 4. Internet Connectivity
- **WiFi:** Direct internet access
- **LoRa:** Requires gateway with internet

## Performance Comparison

| Metric | WiFi | LoRa |
|--------|------|------|
| Range | 50-100m | 1-2 km |
| Battery Life | 4-8 hrs | 24-48 hrs |
| Data Rate | Mbps | 5 kbps |
| Latency | < 1s | 1-5s |
| Setup Complexity | Low | Medium |
| Infrastructure | WiFi AP required | Gateway required |
| Cost | $$ | $$$ |

## Packet Format

The packet format remains unchanged between WiFi and LoRa versions:

### Packet Structure
```
[Device ID (10 bytes)] [Frame Counter (2 bytes)] [Port (1 byte)] [Payload (n bytes)]
```

### Packet Types
1. **Type 1 - Realtime Data (10 bytes payload)**
   - Heart rate, temperature, noise, fall state, alerts

2. **Type 2 - ECG Data (65 bytes payload)**
   - Compressed ECG waveform + PQRST features

3. **Type 3 - Fall Event (45 bytes payload)**
   - Fall detection metrics, accelerometer data

## Troubleshooting

### ESP32 Not Transmitting

1. Check LoRa initialization in serial monitor
2. Verify antenna is connected
3. Check SPI pins are correct
4. Test with lower SF (7 or 8)

### Vision Master Not Receiving

1. Verify LoRa parameters match ESP32
2. Check frequency (915 vs 868 MHz)
3. Verify sync word (0x12)
4. Reduce distance for testing (<10m)

### Raspberry Pi Not Receiving UART

1. Enable UART in raspi-config
2. Check TX/RX wiring (crossed correctly)
3. Verify baud rate (115200)
4. Check permissions: `sudo chmod 666 /dev/ttyS0`

### No Data on Dashboard

1. Check Docker containers: `docker ps`
2. Check API logs: `docker logs health-monitor-api -f`
3. Test API: `curl http://localhost:5000/health`
4. Check Python receiver: `sudo journalctl -u lora-receiver -f`

## Region-Specific Configuration

### North America / Asia
```cpp
const float LORA_FREQUENCY = 915.0;  // MHz
```

### Europe
```cpp
const float LORA_FREQUENCY = 868.0;  // MHz
```

### China
```cpp
const float LORA_FREQUENCY = 470.0;  // MHz
```

**Important:** Verify allowed frequencies and power levels in your region!

## Future Enhancements

### Potential Improvements
1. LoRaWAN support (TTN/Helium network)
2. Mesh networking for multiple devices
3. Adaptive data rate (ADR)
4. Encryption for sensitive health data
5. Over-the-air (OTA) firmware updates
6. Battery monitoring and low-power modes

## References

- RadioLib Documentation: https://github.com/jgromes/RadioLib
- LoRa Alliance: https://lora-alliance.org/
- Vision Master E213: Check manufacturer docs
- Heltec WiFi LoRa 32 V3: https://heltec.org/

## Support

For issues or questions:
1. Check logs: `sudo journalctl -u lora-receiver -f`
2. Review `VISION_MASTER_SETUP.md` for detailed setup
3. Test each component individually
4. Verify all configurations match

## Rollback to WiFi

If you need to revert to WiFi:

1. The original WiFi code is preserved in git history
2. Restore `main.cpp` from previous commit
3. Update `platformio.ini` to remove RadioLib
4. Configure WiFi credentials
5. Upload firmware

## Conclusion

The migration from WiFi to LoRa provides:
- ✅ Longer range (10-20x improvement)
- ✅ Better battery life (3-6x improvement)
- ✅ No WiFi infrastructure required
- ✅ Better penetration through walls
- ✅ Suitable for outdoor/rural deployments

The system architecture remains similar, with the main change being the communication layer. The backend, database, and dashboard remain unchanged, making the migration relatively straightforward.
