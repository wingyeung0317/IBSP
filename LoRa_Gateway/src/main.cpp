/**
 * Vision Master E213 - LoRa Gateway with Display & UART
 * Receives LoRa packets, displays on E-ink, forwards via UART to Raspberry Pi
 */

#include <Arduino.h>
#include <RadioLib.h>
#include "HT_E0213A367.h"

// Vext Power Control (Active HIGH for Vision Master E213)
#define Vext 18

// Vision Master E213 SX1262 LoRa pins (Heltec Official - VERIFIED)
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_NRST   12
#define LORA_BUSY   13
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCLK   9

// E-ink Display pins
#define EPD_RST   3
#define EPD_DC    2
#define EPD_CS    5
#define EPD_BUSY  1
#define EPD_SCLK  4
#define EPD_MOSI  6

// UART to Raspberry Pi
#define UART_TX   44
#define UART_RX   43
#define UART_BAUD 115200

// LoRa Parameters (verified working)
const float LORA_FREQUENCY = 923.0;
const float LORA_BANDWIDTH = 125.0;
const uint8_t LORA_SPREADING_FACTOR = 9;
const uint8_t LORA_CODING_RATE = 7;
const uint8_t LORA_SYNC_WORD = 0x12;
const int8_t LORA_OUTPUT_POWER = 22;
const uint16_t LORA_PREAMBLE_LENGTH = 8;

// Hardware objects
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY, SPI);
ScreenDisplay *display = nullptr;

// State
uint32_t packetsReceived = 0;
uint32_t packetsSkipped = 0;  // Count of skipped unknown packets
uint8_t rxBuffer[256];
uint8_t lastRxBuffer[256];  // Buffer to store last received packet for duplicate detection
int lastRxLength = 0;        // Length of last received packet
bool displayAvailable = false;
int displayWidth = 250;
int displayHeight = 122;
bool needsFullRefresh = true;  // Flag for full refresh on first display
unsigned long lastPacketMillis = 0;  // Last packet received time
uint16_t lastFrameCounter = 0;  // Track last frame counter to detect new packets

// Time sync
struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  bool valid;
  unsigned long lastSyncMillis;
} currentTime = {0, 0, 0, 2025, 1, 1, false, 0};

// Last packet info
struct {
  uint8_t type;  // 1=Realtime, 2=ECG, 3=Fall
  char deviceId[11];
  uint16_t frameCounter;
  int8_t heartRate;
  float temperature;
  uint8_t noiseLevel;
  uint8_t fallState;  // 0=Normal, 1=Warning, 2=Fall, 3=DANGEROUS/Unconscious, 4=Recovery
  bool fallDetected;
  bool noiseAlert;
  bool hrAlert;
  bool tempAlert;
} lastPacket = {0, "", 0, 0, 0.0, 0, false, false, false, false};

// ============================================================================
// TIME SYNC & PACKET PARSING
// ============================================================================

void updateCurrentTime() {
  if (!currentTime.valid) return;
  
  // Calculate elapsed time since last sync
  unsigned long elapsedMs = millis() - currentTime.lastSyncMillis;
  unsigned long elapsedSec = elapsedMs / 1000;
  
  currentTime.second += elapsedSec;
  currentTime.lastSyncMillis += elapsedSec * 1000;
  
  // Handle overflow
  if (currentTime.second >= 60) {
    currentTime.minute += currentTime.second / 60;
    currentTime.second %= 60;
  }
  if (currentTime.minute >= 60) {
    currentTime.hour += currentTime.minute / 60;
    currentTime.minute %= 60;
  }
  if (currentTime.hour >= 24) {
    currentTime.hour %= 24;
    // Note: day/month/year rollover not implemented for simplicity
  }
}

void parsePacketInfo(uint8_t* data, int length) {
  if (length < 13) return;  // Minimum: 10B device ID + 2B frame + 1B port
  
  // Extract device ID (first 10 bytes)
  memcpy(lastPacket.deviceId, data, 10);
  lastPacket.deviceId[10] = '\0';
  
  // Extract frame counter (bytes 10-11, little-endian)
  lastPacket.frameCounter = (data[11] << 8) | data[10];
  
  // Extract port/packet type (byte 12)
  lastPacket.type = data[12];
  
  // Reset alerts
  lastPacket.noiseAlert = false;
  lastPacket.hrAlert = false;
  lastPacket.tempAlert = false;
  
  // Payload starts at byte 13
  if (lastPacket.type == 1 && length >= 23) {
    // Realtime data payload (10 bytes):
    // [0] type=0x01
    // [1] HR
    // [2] body temp
    // [3] ambient temp
    // [4] noise
    // [5] fall state
    // [6] alert flags
    // [7-8] RSSI (placeholder)
    // [9] SNR (placeholder)
    
    lastPacket.heartRate = data[14];  // Byte 1 of payload
    uint8_t tempEncoded = data[15];   // Byte 2 of payload
    lastPacket.temperature = ((tempEncoded / 255.0) * 100.0) - 20.0;
    // Skip ambient temp (byte 16)
    lastPacket.noiseLevel = data[17];  // Byte 4 of payload
    lastPacket.fallState = data[18];   // Byte 5 = fall_state (0-4)
    lastPacket.fallDetected = (lastPacket.fallState >= 2);  // Fall, Dangerous, or Recovery
    
    // Extract alert flags (byte 6 of payload = byte 19 of packet)
    uint8_t alertFlags = data[19];
    lastPacket.hrAlert = (alertFlags & 0x01) != 0;    // Bit 0: HR abnormal
    lastPacket.tempAlert = (alertFlags & 0x02) != 0;  // Bit 1: Temp abnormal
    // Skip bit 2 (fall alert - redundant with fall_state)
    lastPacket.noiseAlert = (alertFlags & 0x08) != 0; // Bit 3: Noise alert
    
  } else if (lastPacket.type == 2) {
    // ECG data
    lastPacket.heartRate = -1;  // Unknown
    lastPacket.temperature = 0;
    lastPacket.noiseLevel = 0;
    lastPacket.fallState = 0;
    lastPacket.fallDetected = false;
    
  } else if (lastPacket.type == 3 && length >= 58) {
    // Fall event: [type][timestamp 4B][jerk 4B][svm 4B][...][HR][temp][...]
    // Payload structure is more complex - adjust offsets
    lastPacket.heartRate = data[40];  // Byte 27 of payload
    uint8_t tempEncoded = data[41];
    lastPacket.temperature = ((tempEncoded / 255.0) * 100.0) - 20.0;
    lastPacket.noiseLevel = 0;  // Not included in fall event
    lastPacket.fallState = 2;  // Fall detected
    lastPacket.fallDetected = true;
  }
}

void checkUartTimeSync() {
  // Time sync format from Raspberry Pi: [0xFF][0xFE][YY][YY][MM][DD][HH][MM][SS][0xFD]
  // Always sent with every packet, no request needed
  while (Serial1.available() >= 10) {
    if (Serial1.peek() == 0xFF) {
      uint8_t syncBuffer[10];
      Serial1.readBytes(syncBuffer, 10);
      
      if (syncBuffer[0] == 0xFF && syncBuffer[1] == 0xFE && syncBuffer[9] == 0xFD) {
        currentTime.year = (syncBuffer[2] << 8) | syncBuffer[3];
        currentTime.month = syncBuffer[4];
        currentTime.day = syncBuffer[5];
        currentTime.hour = syncBuffer[6];
        currentTime.minute = syncBuffer[7];
        currentTime.second = syncBuffer[8];
        currentTime.valid = true;
        currentTime.lastSyncMillis = millis();
        
        Serial.printf("⏰ Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                     currentTime.year, currentTime.month, currentTime.day,
                     currentTime.hour, currentTime.minute, currentTime.second);
        break;
      }
    } else {
      Serial1.read(); // Discard invalid byte
    }
  }
}

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);  // Active HIGH
  delay(100);
}

void VextOFF() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void initDisplay() {
  Serial.println("\n--- Initializing E-ink Display ---");
  
  VextON();
  delay(100);
  
  // Chip ID detection (from Heltec example)
  pinMode(EPD_SCLK, OUTPUT); 
  pinMode(EPD_DC, OUTPUT); 
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  
  // Reset E-ink
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(20);
  
  digitalWrite(EPD_DC, LOW);
  digitalWrite(EPD_CS, LOW);
  
  // Write cmd 0x2F (Read Device Info)
  uint8_t cmd = 0x2F;
  pinMode(EPD_MOSI, OUTPUT);  
  digitalWrite(EPD_SCLK, LOW);
  for (int i = 0; i < 8; i++) {
    digitalWrite(EPD_MOSI, (cmd & 0x80) ? HIGH : LOW);
    cmd <<= 1;
    digitalWrite(EPD_SCLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(EPD_SCLK, LOW);
    delayMicroseconds(1);
  }
  delay(10);
  
  digitalWrite(EPD_DC, HIGH);
  pinMode(EPD_MOSI, INPUT_PULLUP); 
  
  // Read chip ID
  uint8_t chipId = 0;
  for (int8_t b = 7; b >= 0; b--) {
    digitalWrite(EPD_SCLK, LOW);  
    delayMicroseconds(1);
    digitalWrite(EPD_SCLK, HIGH);
    delayMicroseconds(1);
    if (digitalRead(EPD_MOSI)) chipId |= (1 << b);  
  }
  digitalWrite(EPD_CS, HIGH);
  
  Serial.printf("E-ink Chip ID: 0x%02X\n", chipId);
  display = new HT_E0213A367(EPD_RST, EPD_DC, EPD_CS, EPD_BUSY, EPD_SCLK, EPD_MOSI, -1, 6000000);
  
  if (display) {
    display->init();
    display->screenRotate(ANGLE_0_DEGREE);
    display->setFont(ArialMT_Plain_10);
    
    // Boot screen
    display->clear();
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_16);
    display->drawString(10, 20, "Vision Master");
    display->drawString(10, 40, "E213 Gateway");
    
    display->setFont(ArialMT_Plain_10);
    display->drawString(10, 70, "LoRa: 923MHz SF9");
    display->drawString(10, 85, "Initializing...");
    
    display->update(BLACK_BUFFER);
    display->display();
    delay(300);
    
    needsFullRefresh = true;  // Next update will do full refresh
    
    Serial.println("✅ Display initialized");
    displayAvailable = true;
  } else {
    Serial.println("❌ Display init failed");
    displayAvailable = false;
  }
}

void updateDisplay(int rssi, float snr, int length, uint32_t count) {
  if (!displayAvailable || !display) return;
  
  updateCurrentTime();
  
  // First time or critical alert - do full refresh
  if (needsFullRefresh || (lastPacket.fallState == 3) || 
      (lastPacket.fallDetected && lastPacket.type == 3)) {
    
    display->clear();
    
    // === Header: Time and Title ===
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    
    if (currentTime.valid) {
      char timeStr[20];
      sprintf(timeStr, "%02d:%02d:%02d", currentTime.hour, currentTime.minute, currentTime.second);
      display->drawString(2, 0, timeStr);
    } else {
      display->drawString(2, 0, "--:--:--");
    }
    
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_16);
    display->drawString(125, 0, "LoRa Gateway");
    
    // === Line separator ===
    display->drawHorizontalLine(0, 18, 250);
    
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    char buffer[60];
    
    if (count > 0) {
      // === LEFT COLUMN (0-120) ===
      
      // Packet type
      const char* typeStr = "Unknown";
      if (lastPacket.type == 1) typeStr = "Realtime";
      else if (lastPacket.type == 2) typeStr = "ECG";
      else if (lastPacket.type == 3) typeStr = "FALL";
      
      sprintf(buffer, "Type:%s", typeStr);
      display->drawString(2, 22, buffer);
      
      // Device ID
      sprintf(buffer, "Dev:%.8s", lastPacket.deviceId);
      display->drawString(2, 34, buffer);
      
      // Frame number with skipped packets indicator
      // Show (+x) if packets were skipped before this valid packet
      if (packetsSkipped > 0) {
        sprintf(buffer, "Frame:#%d(+%lu)", lastPacket.frameCounter, packetsSkipped);
      } else {
        sprintf(buffer, "Frame:#%d", lastPacket.frameCounter);
      }
      display->drawString(2, 46, buffer);
      
      // Health data
      if (lastPacket.type == 1 || lastPacket.type == 3) {
        sprintf(buffer, "HR:%d%s bpm", 
                lastPacket.heartRate,
                lastPacket.hrAlert ? "!" : "");
        display->drawString(2, 58, buffer);
        
        sprintf(buffer, "Temp:%.1f%sC", 
                lastPacket.temperature,
                lastPacket.tempAlert ? "!" : "");
        display->drawString(2, 70, buffer);
        
        if (lastPacket.type == 1) {
          sprintf(buffer, "Noise:%ddB%s", 
                  lastPacket.noiseLevel,
                  lastPacket.noiseAlert ? "!" : "");
          display->drawString(2, 82, buffer);
        }
      }
      
      // === RIGHT COLUMN (130-248) ===
      
      // Signal quality
      sprintf(buffer, "RSSI:%ddBm", rssi);
      display->drawString(130, 22, buffer);
      
      sprintf(buffer, "SNR:%.1fdB", snr);
      display->drawString(130, 34, buffer);
      
      // Packet stats
      sprintf(buffer, "Size:%dB", length);
      display->drawString(130, 46, buffer);
      
      unsigned long uptime = millis() / 1000;
      unsigned long hours = uptime / 3600;
      unsigned long minutes = (uptime % 3600) / 60;
      unsigned long seconds = uptime % 60;
      sprintf(buffer, "Up:%luh%02lum%02lus", hours, minutes, seconds);
      display->drawString(130, 58, buffer);
      
      // Alert status
      if (lastPacket.fallState == 3) {
        // DANGEROUS - Unconscious/Immobile
        display->setFont(ArialMT_Plain_16);
        display->drawString(130, 68, "UNCONSCIOUS!");
        display->setFont(ArialMT_Plain_10);
      } else if (lastPacket.fallDetected) {
        display->setFont(ArialMT_Plain_16);
        display->drawString(130, 72, "**FALL**");
        display->setFont(ArialMT_Plain_10);
      } else if (lastPacket.noiseAlert) {
        display->drawString(130, 70, "LOUD!");
      }  else {
        display->drawString(130, 70, "Normal");
      }
      
      // === Bottom status bar ===
      display->drawHorizontalLine(0, 96, 250);
      
      if (lastPacket.fallState == 3) {
        // Critical: Unconscious warning
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(125, 100, "!! UNCONSCIOUS !!");
      } else if (lastPacket.fallDetected && lastPacket.type == 3) {
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(125, 100, ">> FALL EVENT <<");
      } else {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(ArialMT_Plain_10);
        sprintf(buffer, "Listening: 923MHz SF9");
        display->drawString(2, 100, buffer);
        
        // Show last packet time in right corner
        if (lastPacketMillis > 0) {
          unsigned long elapsed = (millis() - lastPacketMillis) / 1000;
          display->setTextAlignment(TEXT_ALIGN_RIGHT);
          if (elapsed < 60) {
            sprintf(buffer, "RX:(%02lu)s ago", elapsed);
          } else if (elapsed < 3600) {
            sprintf(buffer, "RX:(%02lu)m ago", elapsed / 60);
          } else {
            sprintf(buffer, "RX:(%02lu)h ago", elapsed / 3600);
          }
          display->drawString(248, 100, buffer);
        }
      }
      
    } else {
      // === No packets yet ===
      display->setTextAlignment(TEXT_ALIGN_CENTER);
      display->setFont(ArialMT_Plain_16);
      display->drawString(125, 50, "Waiting...");
      display->setFont(ArialMT_Plain_10);
      display->drawString(125, 70, "923MHz SF9 BW125");
    }
    
    display->update(BLACK_BUFFER);
    display->display();  // Full refresh

    // Reset skipped counter after showing in full refresh
    packetsSkipped = 0;

    if(lastPacket.fallDetected){
      needsFullRefresh = true;
    }else{
      needsFullRefresh = false;
    }
    
  } else {
    // Partial refresh - only update dynamic content
    // This is much faster and reduces flicker
    
    display->setColor(BLACK);  // Clear area
    display->fillRect(2, 0, 60, 12);  // Time area
    display->setColor(WHITE);
    
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    
    // Update time
    if (currentTime.valid) {
      char timeStr[20];
      sprintf(timeStr, "%02d:%02d:%02d", currentTime.hour, currentTime.minute, currentTime.second);
      display->drawString(2, 0, timeStr);
    } else {
      display->drawString(2, 0, "--:--:--");
    }
    
    // Update dynamic data fields
    char buffer[60];
    
    if (count > 0) {
      // Clear and update left column data
      display->setColor(BLACK);
      display->fillRect(2, 46, 120, 48);  // Frame, HR, Temp, Noise area
      display->setColor(WHITE);
      
      // Show (-x) when waiting for new valid packet with x skipped packets
      if (packetsSkipped > 0) {
        sprintf(buffer, "Frame:#%d(-%lu)", lastPacket.frameCounter, packetsSkipped);
      } else {
        sprintf(buffer, "Frame:#%d", lastPacket.frameCounter);
      }
      display->drawString(2, 46, buffer);
      
      if (lastPacket.type == 1 || lastPacket.type == 3) {
        sprintf(buffer, "HR:%d%s bpm", 
                lastPacket.heartRate,
                lastPacket.hrAlert ? "!" : "");
        display->drawString(2, 58, buffer);
        
        sprintf(buffer, "Temp:%.1f%sC", 
                lastPacket.temperature,
                lastPacket.tempAlert ? "!" : "");
        display->drawString(2, 70, buffer);
        
        if (lastPacket.type == 1) {
          sprintf(buffer, "Noise:%ddB%s", 
                  lastPacket.noiseLevel,
                  lastPacket.noiseAlert ? "!" : "");
          display->drawString(2, 82, buffer);
        }
      }
      
      // Clear and update right column
      display->setColor(BLACK);
      display->fillRect(130, 22, 118, 72);  // Signal and status area
      display->setColor(WHITE);
      
      sprintf(buffer, "RSSI:%ddBm", rssi);
      display->drawString(130, 22, buffer);
      
      sprintf(buffer, "SNR:%.1fdB", snr);
      display->drawString(130, 34, buffer);
      
      sprintf(buffer, "Size:%dB", length);
      display->drawString(130, 46, buffer);
      
      unsigned long uptime = millis() / 1000;
      unsigned long hours = uptime / 3600;
      unsigned long minutes = (uptime % 3600) / 60;
      unsigned long seconds = uptime % 60;
      sprintf(buffer, "Up:%luh%02lum%02lus", hours, minutes, seconds);
      display->drawString(130, 58, buffer);
      
      // Alert status
      if (lastPacket.noiseAlert) {
        display->drawString(130, 70, "LOUD!");
      } else if (lastPacket.hrAlert || lastPacket.tempAlert) {
        display->drawString(130, 70, "Alert!");
      } else {
        display->drawString(130, 70, "Normal");
      }
      
      // Update bottom status bar - last packet time
      display->setColor(BLACK);
      display->fillRect(150, 100, 98, 12);  // Clear right side of status bar
      display->setColor(WHITE);
      
      if (lastPacketMillis > 0) {
        unsigned long elapsed = (millis() - lastPacketMillis) / 1000;
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        if (elapsed < 60) {
          sprintf(buffer, "RX:(%02lu)s ago", elapsed);
        } else if (elapsed < 3600) {
          sprintf(buffer, "RX:(%02lu)m ago", elapsed / 60);
        } else {
          sprintf(buffer, "RX:(%lu)h ago", elapsed / 3600);
        }
        display->drawString(248, 100, buffer);
      }
    }
    
    // Use partial refresh for faster updates
    display->update(BLACK_BUFFER);
    ((HT_E0213A367*)display)->displayPartial();
  }
}

// ============================================================================
// UART FORWARDING
// ============================================================================

void forwardToRaspberryPi(uint8_t* data, int length, int rssi, float snr) {
  // Frame format: [0xAA][LEN][RSSI][SNR][DATA...][0x55]
  Serial1.write(0xAA);  // Start marker
  Serial1.write((uint8_t)length);
  Serial1.write((uint8_t)(rssi + 150));  // Convert -150~0 to 0~150
  Serial1.write((uint8_t)(snr + 20));    // Convert -20~20 to 0~40
  Serial1.write(data, length);
  Serial1.write(0x55);  // End marker
  
  Serial.printf("   → Forwarded to UART (%d bytes)\n", length + 5);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for USB CDC
  
  Serial.println("\n========================================");
  Serial.println("Vision Master E213 - LoRa Gateway");
  Serial.println("========================================");
  Serial.println("Starting...");
  
  // Initialize UART to Raspberry Pi
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  Serial.println("✅ UART initialized (to Raspberry Pi)");
  
  // Initialize SPI for LoRa
  Serial.println("Initializing SPI...");
  SPI.begin(LORA_SCLK, LORA_MISO, LORA_MOSI, LORA_NSS);
  Serial.println("✅ SPI OK");
  
  // Initialize E-ink Display
  initDisplay();
  
  // Initialize LoRa (VERIFIED WORKING CONFIG)
  Serial.println("\nInitializing SX1262...");
  Serial.printf("NSS=%d DIO1=%d RST=%d BUSY=%d\n", LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);
  
  int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, 
                         LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER, 
                         LORA_PREAMBLE_LENGTH);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ LoRa initialized successfully!");
    Serial.printf("Frequency: %.1f MHz\n", LORA_FREQUENCY);
    Serial.printf("SF: %d, BW: %.1f kHz\n", LORA_SPREADING_FACTOR, LORA_BANDWIDTH);
  } else {
    Serial.printf("❌ LoRa init failed, code: %d\n", state);
    
    // Update display with error
    if (displayAvailable && display) {
      display->clear();
      display->setFont(ArialMT_Plain_10);
      display->setTextAlignment(TEXT_ALIGN_LEFT);
      char buffer[50];
      display->drawString(10, 50, "LoRa Init Failed!");
      sprintf(buffer, "Error code: %d", state);
      display->drawString(10, 65, buffer);
      display->update(BLACK_BUFFER);
      display->display();
    }
    
    while (true) {
      delay(1000);
    }
  }
  
  // Start receiving
  Serial.println("\nStarting receiver mode...");
  state = radio.startReceive();
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ Receiver started");
  } else {
    Serial.printf("❌ Start failed, code: %d\n", state);
  }
  
  // Update display - ready state
  if (displayAvailable) {
    updateDisplay(0, 0, 0, 0);
  }
  
  Serial.println("\n🎧 Listening for packets...\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Check for time sync from Raspberry Pi (sent with every packet)
  checkUartTimeSync();
  
  int state = radio.getPacketLength();
  
  if (state > 0) {
    state = radio.readData(rxBuffer, sizeof(rxBuffer));
    
    if (state == RADIOLIB_ERR_NONE) {
      int len = radio.getPacketLength();
      int rssi = radio.getRSSI();
      float snr = radio.getSNR();
      
      // Check packet type before parsing to preserve last valid packet
      if (len >= 13) {
        uint8_t packetType = rxBuffer[12];  // Port/packet type at byte 12
        
        // Ignore unknown packet types - keep last valid packet
        if (packetType == 0 || packetType > 3) {
          // Check if this is a duplicate of the last bad packet
          bool isDuplicate = false;
          if (lastRxLength == len && len > 0) {
            isDuplicate = true;
            for (int i = 0; i < len; i++) {
              if (rxBuffer[i] != lastRxBuffer[i]) {
                isDuplicate = false;
                break;
              }
            }
          }
          
          // Only count and update display for new (non-duplicate) bad packets
          if (!isDuplicate) {
            packetsSkipped++;
            Serial.printf("\n⚠️ Unknown packet type: %d - Skipped #%lu (keeping last valid packet)\n", packetType, packetsSkipped);
            
            // Store this bad packet for duplicate detection
            memcpy(lastRxBuffer, rxBuffer, len);
            lastRxLength = len;
            
            // Update display to show skipped count
            if (displayAvailable && packetsReceived > 0) {
              updateDisplay(rssi, snr, len, packetsReceived);
            }
          } else {
            Serial.printf("\n🔁 Duplicate bad packet (type: %d) - Ignored\n", packetType);
          }
          
          radio.startReceive();
          return;
        }
      } else {
        // Packet too short, skip it
        // Check if this is a duplicate
        bool isDuplicate = false;
        if (lastRxLength == len && len > 0) {
          isDuplicate = true;
          for (int i = 0; i < len; i++) {
            if (rxBuffer[i] != lastRxBuffer[i]) {
              isDuplicate = false;
              break;
            }
          }
        }
        
        if (!isDuplicate) {
          packetsSkipped++;
          Serial.printf("\n⚠️ Packet too short (%d bytes) - Skipped #%lu\n", len, packetsSkipped);
          
          // Store this bad packet
          memcpy(lastRxBuffer, rxBuffer, len);
          lastRxLength = len;
          
          // Update display to show skipped count
          if (displayAvailable && packetsReceived > 0) {
            updateDisplay(radio.getRSSI(), radio.getSNR(), len, packetsReceived);
          }
        } else {
          Serial.printf("\n🔁 Duplicate short packet (%d bytes) - Ignored\n", len);
        }
        
        radio.startReceive();
        return;
      }
      
      // Parse packet information (only if type is valid)
      parsePacketInfo(rxBuffer, len);
      
      // Update counters only when frame counter changes (new packet)
      if (lastPacket.frameCounter != lastFrameCounter) {
        packetsReceived++;
        needsFullRefresh = true;
        lastPacketMillis = millis();
        lastFrameCounter = lastPacket.frameCounter;
        
        // Trigger full refresh if packets were skipped to show (+x)
        if (packetsSkipped > 0) {
          Serial.printf("   ℹ️ Skipped %lu unknown packet(s) before this valid packet\n", packetsSkipped);
          needsFullRefresh = true;  // Force full refresh to show (+x)
        }
        
        // Note: packetsSkipped will be reset to 0 after display update in full refresh
      }
      
      // Check if layout needs full refresh (packet type changed, or critical alert)
      static uint8_t lastPacketType = 0;
      if (lastPacket.type != lastPacketType || 
          lastPacket.fallState == 3 || 
          (lastPacket.fallDetected && lastPacket.type == 3)) {
        needsFullRefresh = true;
        lastPacketType = lastPacket.type;
      }
      
      // Print to Serial
      Serial.printf("\n📦 Packet #%lu\n", packetsReceived);
      Serial.printf("   Type: %d, Device: %s, Frame: %d\n", 
                    lastPacket.type, lastPacket.deviceId, lastPacket.frameCounter);
      Serial.printf("   Length: %d bytes\n", len);
      Serial.printf("   RSSI: %d dBm, SNR: %.2f dB\n", rssi, snr);
      
      if (lastPacket.type == 1 || lastPacket.type == 3) {
        Serial.printf("   HR: %d bpm%s, Temp: %.1f°C%s", 
                     lastPacket.heartRate,
                     lastPacket.hrAlert ? " [ABNORMAL]" : "",
                     lastPacket.temperature,
                     lastPacket.tempAlert ? " [ABNORMAL]" : "");
        if (lastPacket.fallDetected) Serial.print(" [FALL]");
        Serial.println();
        
        if (lastPacket.type == 1) {
          Serial.printf("   Noise: %d dB%s\n", 
                       lastPacket.noiseLevel,
                       lastPacket.noiseAlert ? " [TOO LOUD]" : "");
        }
      }
      
      Serial.print("   Data: ");
      for (int i = 0; i < min(len, 20); i++) {
        Serial.printf("%02X ", rxBuffer[i]);
      }
      if (len > 20) Serial.print("...");
      Serial.println();
      
      // Forward to Raspberry Pi via UART
      forwardToRaspberryPi(rxBuffer, len, rssi, snr);
      
      // Update E-ink display
      updateDisplay(rssi, snr, len, packetsReceived);
      
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
      Serial.printf("❌ Read error: %d\n", state);
    }
    
    radio.startReceive();
  }
  
  // Print heartbeat every 10 seconds
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    updateCurrentTime();
    Serial.printf("[Heartbeat] Running... RX: %lu", packetsReceived);
    if (currentTime.valid) {
      Serial.printf(" Time: %02d:%02d:%02d", currentTime.hour, currentTime.minute, currentTime.second);
    }
    Serial.println();
  }
}