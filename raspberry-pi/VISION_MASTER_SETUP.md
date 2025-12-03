# Vision Master E213 Configuration Guide
# ========================================

## Hardware Setup

### Vision Master E213 as LoRa Receiver
The Vision Master E213 is a powerful LoRa device that can receive packets and forward them via UART.

**Specifications:**
- LoRa Module: SX1262
- Frequency: 470-510 MHz / 860-930 MHz (depending on version)
- UART Interface: Available on GPIO pins
- Display: For monitoring received packets
- Power: USB-C or battery

### Connections to Raspberry Pi

```
Vision Master E213          Raspberry Pi
==================          ============
TX (GPIO)          ------>  RX (GPIO 15, Pin 10)
RX (GPIO)          <------  TX (GPIO 14, Pin 8)
GND                ------>  GND (Pin 6 or 14)
VCC (5V)           ------>  5V (Pin 2 or 4)
```

**Important:**
- Ensure voltage levels are compatible (3.3V or 5V)
- Use level shifters if needed
- Connect GND first before power

### Raspberry Pi UART Configuration

1. **Enable UART in Raspberry Pi**:
   ```bash
   sudo raspi-config
   ```
   - Navigate to: `Interface Options` → `Serial Port`
   - Disable serial login shell: **No**
   - Enable serial hardware: **Yes**
   - Reboot

2. **Check UART ports**:
   ```bash
   ls -l /dev/tty*
   ```
   Common ports:
   - `/dev/ttyS0` - Primary UART (Raspberry Pi 4/5)
   - `/dev/ttyAMA0` - Alternative UART (Raspberry Pi 3)

3. **Set permissions**:
   ```bash
   sudo usermod -a -G dialout $USER
   sudo chmod 666 /dev/ttyS0
   ```

4. **Verify connection**:
   ```bash
   sudo minicom -D /dev/ttyS0 -b 115200
   ```

## Vision Master E213 Configuration

### Receiver Mode Configuration

The Vision Master E213 needs to be configured in **LoRa Receiver Mode** with UART output.

**Configuration Parameters (must match ESP32):**

```
Frequency: 915.0 MHz (or your region)
Bandwidth: 125 kHz
Spreading Factor: 9
Coding Rate: 4/7
Sync Word: 0x12 (private network)
TX Power: 22 dBm (for sender)
Preamble Length: 8
```

### Programming Vision Master E213

You can program the Vision Master E213 using:

1. **Arduino IDE** (recommended for beginners)
2. **PlatformIO** (for advanced users)
3. **ESP-IDF** (for custom firmware)

**Sample Receiver Code** (for Vision Master E213):

```cpp
#include <Arduino.h>
#include <RadioLib.h>

// Vision Master E213 SX1262 pins (verify your model)
#define LORA_NSS    10
#define LORA_DIO1   5
#define LORA_NRST   3
#define LORA_BUSY   36

// UART for Raspberry Pi
#define UART_TX     1
#define UART_RX     2
#define UART_BAUD   115200

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

void setup() {
  // Initialize UART for Raspberry Pi
  Serial.begin(UART_BAUD);
  
  // Initialize LoRa
  int state = radio.begin(915.0, 125.0, 9, 7, 0x12, 22, 8);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa init OK");
  } else {
    Serial.print("LoRa init failed: ");
    Serial.println(state);
    while(true);
  }
  
  // Start receiving
  radio.startReceive();
}

void loop() {
  // Check if packet received
  if (radio.getPacketLength() > 0) {
    uint8_t buffer[256];
    int state = radio.readData(buffer, 256);
    
    if (state == RADIOLIB_ERR_NONE) {
      int len = radio.getPacketLength();
      int rssi = radio.getRSSI();
      float snr = radio.getSNR();
      
      // Forward to Raspberry Pi via UART
      // Format: [Length(2)] [RSSI(2)] [SNR(2)] [Data(n)]
      Serial.write((len >> 8) & 0xFF);
      Serial.write(len & 0xFF);
      Serial.write((rssi >> 8) & 0xFF);
      Serial.write(rssi & 0xFF);
      
      int16_t snr_int = (int16_t)(snr * 100);
      Serial.write((snr_int >> 8) & 0xFF);
      Serial.write(snr_int & 0xFF);
      
      Serial.write(buffer, len);
      Serial.flush();
    }
    
    // Continue receiving
    radio.startReceive();
  }
  
  delay(10);
}
```

### Building and Uploading

**Using PlatformIO:**

```ini
[env:vision_master_e213]
platform = espressif32
board = esp32-s3-devkitc-1  ; Or specific Vision Master board
framework = arduino
lib_deps = 
    jgromes/RadioLib@^6.4.0
monitor_speed = 115200
```

**Using Arduino IDE:**
1. Install RadioLib library: `Tools` → `Manage Libraries` → Search "RadioLib"
2. Select board: `Tools` → `Board` → `ESP32 S3 Dev Module` (or Vision Master)
3. Connect via USB
4. Upload code

## Testing the Setup

### 1. Test UART Communication

```bash
# On Raspberry Pi, read raw UART data
cat /dev/ttyS0
```

### 2. Test LoRa Reception

```bash
# Start the Python receiver script
cd /home/pi/IBSP/raspberry-pi
python3 uart_lora_receiver.py
```

Expected output:
```
🚀 LoRa UART Receiver Starting
✅ UART port opened: /dev/ttyS0 @ 115200 baud
✅ Server is reachable
🎧 Listening for LoRa packets...

📦 Packet #1
   Device: ESP32-001
   Type: Realtime (Port 1)
   Frame: 0
   Size: 10 bytes
   ✅ Sent to server (HTTP 200)
```

### 3. Monitor System Logs

```bash
# Check Docker logs
docker logs health-monitor-api -f

# Check system UART
dmesg | grep tty
```

## Troubleshooting

### No UART Data Received

1. **Check wiring**:
   ```bash
   # Test loopback (connect TX to RX)
   echo "test" > /dev/ttyS0
   cat /dev/ttyS0
   ```

2. **Check UART enabled**:
   ```bash
   sudo raspi-config
   # Interface Options → Serial Port
   ```

3. **Check permissions**:
   ```bash
   ls -l /dev/ttyS0
   sudo chmod 666 /dev/ttyS0
   ```

### Vision Master E213 Not Receiving

1. **Verify LoRa parameters match** between ESP32 and Vision Master
2. **Check frequency**: Must match your region (915 MHz US, 868 MHz EU)
3. **Check antenna connection**
4. **Verify sync word**: 0x12 (private network)
5. **Check range**: Start with < 10m for testing

### Python Script Errors

1. **Install dependencies**:
   ```bash
   pip3 install pyserial requests
   ```

2. **Check Python version**:
   ```bash
   python3 --version  # Should be 3.7+
   ```

3. **Test server connection**:
   ```bash
   curl http://localhost:5000/health
   ```

## Performance Tuning

### LoRa Range vs Speed

| SF | Range  | Speed    | Use Case           |
|----|--------|----------|--------------------|
| 7  | Short  | Fast     | Indoor, high data  |
| 9  | Medium | Balanced | General purpose    |
| 12 | Long   | Slow     | Long range, low data|

**Current configuration (SF9):**
- Range: ~1-2 km line of sight
- Data rate: ~5.5 kbps
- Suitable for health monitoring

### Packet Transmission Timing

**Recommended intervals:**
- Realtime data: Every 60 seconds (1 min)
- ECG data: Every 306 seconds (5.1 min)
- Fall events: Immediate (on detection)

**Why these intervals?**
- Balances battery life and data freshness
- Avoids LoRa duty cycle limits (1% in EU, 36s per hour)
- Reduces packet collisions

## Region-Specific Settings

### North America / Asia
```cpp
const float LORA_FREQUENCY = 915.0;  // 902-928 MHz ISM band
```

### Europe
```cpp
const float LORA_FREQUENCY = 868.0;  // 863-870 MHz ISM band
```

### China
```cpp
const float LORA_FREQUENCY = 470.0;  // 470-510 MHz band
```

**Important:** Check your local regulations for allowed frequencies and power levels!

## Systemd Service (Auto-start on Boot)

Create `/etc/systemd/system/lora-receiver.service`:

```ini
[Unit]
Description=LoRa UART Receiver
After=network.target docker.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/IBSP/raspberry-pi
ExecStart=/usr/bin/python3 /home/pi/IBSP/raspberry-pi/uart_lora_receiver.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable lora-receiver
sudo systemctl start lora-receiver
sudo systemctl status lora-receiver
```

## Monitoring

### Real-time Log Viewing

```bash
# Receiver logs
sudo journalctl -u lora-receiver -f

# API server logs
docker logs health-monitor-api -f

# Combined view
docker logs health-monitor-api -f &
sudo journalctl -u lora-receiver -f
```

### Statistics Dashboard

The Python script prints statistics periodically. Press Ctrl+C for summary:

```
📊 RECEPTION STATISTICS
Packets Received: 42
Packets Sent to Server: 42
Errors: 0
Last Packet: 2025-12-04 14:32:15
```

## References

- RadioLib Documentation: https://github.com/jgromes/RadioLib
- Vision Master E213: Check manufacturer documentation
- LoRa Specification: https://lora-alliance.org/
- Raspberry Pi UART: https://www.raspberrypi.com/documentation/computers/configuration.html#configuring-uarts
