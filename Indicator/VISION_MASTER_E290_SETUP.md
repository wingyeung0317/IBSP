# Vision Master E290 Staff Badge Integration

## Hardware Setup

### Components
1. **Heltec Vision Master E290** - Staff badge with E-ink display
2. **Heltec Wireless Stick V3** - Health monitoring device (ESP32)

### Wiring Connection

Connect Vision Master E290 to Wireless Stick V3 via UART:

```
Vision Master E290          Wireless Stick V3 (ESP folder)
─────────────────────────────────────────────────────────
Pin 44 (RX)    ────────────► Pin 43 (TX)
Pin 43 (TX)    ────────────► Pin 44 (RX)
GND            ────────────► GND
```

**Note**: Cross-connect TX to RX

## Software Configuration

### 1. Wireless Stick V3 (ESP - Health Monitor)

**File**: `c:\Users\user\Code\Project\IBSP\esp\src\main.cpp`

Add these modifications manually:

#### A. Add UART Pin Definitions (after LoRa pins, ~line 2000)
```cpp
// UART pins for Vision Master E290 communication
const int PIN_UART_TX = 43;  // Connect to Vision Master E290 RX (pin 44)
const int PIN_UART_RX = 44;  // Connect to Vision Master E290 TX (pin 43)
const int UART_BAUD = 115200;
```

#### B. Add UART Send Function (after PayloadBuilder class, ~line 2300)
```cpp
// ============================================================================
// UART COMMUNICATION TO VISION MASTER E290
// ============================================================================

/**
 * Send realtime data packet to Vision Master E290 via UART
 * Uses same packet format as LoRa for consistency
 */
void sendUARTPacket(uint8_t* payload, int len) {
  if (len > 0 && len <= 255) {
    // Send packet via UART (Serial1)
    Serial1.write(payload, len);
    Serial1.flush();
    
    Serial.println("📤 Sent to Vision Master E290 via UART");
    Serial.print("   Bytes: ");
    Serial.println(len);
  }
}
```

#### C. Initialize UART in setup() (after LoRa init, ~line 2550)
```cpp
  // ========================================
  // UART Initialization for Vision Master E290
  // ========================================
  
  Serial.println("\nInitializing UART for Vision Master E290...");
  Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  Serial.printf("UART configured: TX=%d, RX=%d, Baud=%d\n", 
                PIN_UART_TX, PIN_UART_RX, UART_BAUD);
  Serial.println("✅ UART ready for staff badge communication!");
  Serial.println("========================================\n");
```

#### D. Send via UART in loop() - After LoRa Transmission (~line 3200)
Find this block:
```cpp
      Serial.print("  📶 LoRa RSSI: ");
      Serial.print(loraComm.getRSSI());
      Serial.println(" dBm");
      lastRealtimeTxTime = currentTime;
```

Change to:
```cpp
      Serial.print("  📶 LoRa RSSI: ");
      Serial.print(loraComm.getRSSI());
      Serial.println(" dBm");
      
      // Send same packet to Vision Master E290 via UART
      sendUARTPacket(payload, len);
      
      lastRealtimeTxTime = currentTime;
```

#### E. Send via UART for Emergency States (~line 2970)
Find this block:
```cpp
      Serial.print("  Temp: ");
      Serial.print(tempSensor.currentTemp, 1);
      Serial.print("°C  Noise: ");
      Serial.print(noiseToSend, 1);
      Serial.println("dB");
    } else {
```

Change to:
```cpp
      Serial.print("  Temp: ");
      Serial.print(tempSensor.currentTemp, 1);
      Serial.print("°C  Noise: ");
      Serial.print(noiseToSend, 1);
      Serial.println("dB");
      
      // Send same packet to Vision Master E290 via UART for badge display
      sendUARTPacket(payload, len);
    } else {
```

### 2. Vision Master E290 (Indicator - Staff Badge)

**File**: `c:\Users\user\Documents\PlatformIO\Projects\Indicator\platformio.ini`

Already configured for Vision Master E290!

**File**: `c:\Users\user\Documents\PlatformIO\Projects\Indicator\src\main.cpp`

Complete staff badge program already created!

## Features

### Normal Mode
The E-ink display shows:
- **Staff Name**: Wing (configurable via `STAFF_NAME`)
- **Staff Title**: Healthcare Staff
- **Current Status**: Heart rate, temperature, and status
- **Device ID**: BADGE-001

Display updates every 30 seconds to save power.

### Emergency Mode
When fall (state=2) or unconscious (state=3) detected:
- **Full-screen alert**: Black background with white text
- **Blinking display**: Alternates every second
- **Large warning text**: "FALL ALERT" or "UNCONSCIOUS"
- **Staff name prominently displayed**
- **Current vitals**: Heart rate and temperature
- **Alert message**: "IMMEDIATE ASSISTANCE REQUIRED"

## Data Protocol

### Realtime Packet (10 bytes)
```
Byte 0: Packet type (0x01)
Byte 1: Heart rate (0-255 BPM)
Byte 2: Body temperature (encoded: -20°C to 80°C)
Byte 3: Ambient temperature (encoded)
Byte 4: Noise level (0-255 dB)
Byte 5: Fall state (0=Normal, 1=Warning, 2=Fall, 3=Dangerous, 4=Recovery)
Byte 6: Alert flags
Byte 7-9: RSSI/SNR data
```

### Temperature Decoding
```cpp
float temp = (encoded * 100.0 / 255.0) - 20.0;
```

## Deployment

### Upload to Vision Master E290
```bash
cd c:\Users\user\Documents\PlatformIO\Projects\Indicator
pio run -t upload
```

### Upload to Wireless Stick V3
```bash
cd c:\Users\user\Code\Project\IBSP\esp
pio run -t upload
```

### Monitor Serial Output

**Wireless Stick V3** (to see UART transmissions):
```bash
pio device monitor -p COM_PORT
```

**Vision Master E290** (to see received data):
```bash
pio device monitor -p COM_PORT
```

## Customization

### Change Staff Name
Edit `Indicator/src/main.cpp`:
```cpp
#define STAFF_NAME        "Your Name"
#define STAFF_TITLE       "Your Title"
#define DEVICE_ID         "BADGE-XXX"
```

### Adjust Timing
```cpp
#define NORMAL_REFRESH_MS     30000   // Normal display update interval
#define EMERGENCY_BLINK_MS    1000    // Emergency blink speed
#define UART_TIMEOUT_MS       5000    // Data timeout before "no data" display
```

## Troubleshooting

### No Data on Badge
1. Check UART connections (TX ↔ RX cross-connected)
2. Verify both devices powered on
3. Check serial monitor for "📤 Sent to Vision Master E290 via UART" messages
4. Ensure both use same baud rate (115200)

### Display Not Updating
1. Press button (GPIO 0) for manual refresh
2. Check if emergency mode is active (state 2 or 3)
3. Verify E-ink display connections

### Emergency Not Triggering
1. Check fall_state value in serial monitor
2. Verify UART data is being received (check "📦 Received" messages)
3. Test with manual fall trigger on Wireless Stick V3

## Testing

### Test Normal Display
1. Upload code to both devices
2. Connect UART wires
3. Power on both devices
4. Wait for initial display update (~5 seconds)
5. Should see staff name and "Waiting for sensor data..."

### Test Emergency Alert
1. Trigger fall detection on Wireless Stick V3
2. Watch serial monitor for state change to 2 or 3
3. Vision Master E290 should:
   - Display "🚨 ENTERING EMERGENCY MODE!" in serial
   - Show black/white blinking display
   - Update every second

### Test Recovery
1. Keep device still after fall
2. Wait for state to change to 4 (Recovery) or 0 (Normal)
3. Display should return to normal mode
4. See "✅ Returning to normal mode" in serial

## Pin Reference

### Vision Master E290
- **E-ink SPI**: CS=4, DC=5, RST=6, BUSY=7, MOSI=10, SCK=9
- **UART**: TX=43, RX=44
- **Button**: GPIO 0

### Wireless Stick V3 (ESP folder)
- **UART**: TX=43, RX=44
- **LoRa**: NSS=8, DIO1=14, RESET=12, BUSY=13
- **I2C**: SDA=41, SCL=42
- **Other sensors**: Various GPIOs (see main.cpp)

## Power Consumption

- **Normal mode**: ~1-2 mA (E-ink idle)
- **Display update**: ~20-30 mA (brief burst)
- **Emergency blink**: ~15-25 mA average (frequent updates)

E-ink display consumes power only during updates, making this ideal for battery-powered badges.

## Next Steps

1. Modify ESP32 code to add UART send functions
2. Upload both programs
3. Connect UART wires
4. Test with real sensor data
5. Customize staff name and appearance
6. Deploy in healthcare environment
