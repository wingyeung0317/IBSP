/**
 * Vision Master E213 - LoRa Gateway with Display & UART
 * Receives LoRa packets, displays on E-ink, forwards via UART to Raspberry Pi
 */

#include <Arduino.h>
#include <RadioLib.h>
#include "HT_lCMEN2R13EFC1.h"
#include "HT_E0213A367.h"
#include <Adafruit_GFX.h>

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
uint8_t rxBuffer[256];
bool displayAvailable = false;
int displayWidth = 250;
int displayHeight = 122;

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
  
  // Initialize correct driver based on chip ID
  if ((chipId & 0x03) != 0x01) {
    Serial.println("Detected: HT_ICMEN2R13EFC1");
    display = new HT_ICMEN2R13EFC1(EPD_RST, EPD_DC, EPD_CS, EPD_BUSY, EPD_SCLK, EPD_MOSI, -1, 6000000);
  } else {
    Serial.println("Detected: HT_E0213A367");
    display = new HT_E0213A367(EPD_RST, EPD_DC, EPD_CS, EPD_BUSY, EPD_SCLK, EPD_MOSI, -1, 6000000);
  }
  
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
    
    Serial.println("✅ Display initialized");
    displayAvailable = true;
  } else {
    Serial.println("❌ Display init failed");
    displayAvailable = false;
  }
}

void updateDisplay(int rssi, float snr, int length, uint32_t count) {
  if (!displayAvailable || !display) return;
  
  display->clear();
  
  // Title
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(5, 5, "LoRa Gateway");
  
  // Status
  display->setFont(ArialMT_Plain_10);
  char buffer[50];
  
  sprintf(buffer, "Packets: %lu", count);
  display->drawString(5, 30, buffer);
  
  sprintf(buffer, "RSSI: %d dBm", rssi);
  display->drawString(5, 45, buffer);
  
  sprintf(buffer, "SNR: %.1f dB", snr);
  display->drawString(5, 60, buffer);
  
  sprintf(buffer, "Length: %d bytes", length);
  display->drawString(5, 75, buffer);
  
  // Uptime
  unsigned long uptime = millis() / 1000;
  sprintf(buffer, "Up: %lum %lus", uptime / 60, uptime % 60);
  display->drawString(5, 95, buffer);
  
  display->update(BLACK_BUFFER);
  display->display();
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
  int state = radio.getPacketLength();
  
  if (state > 0) {
    state = radio.readData(rxBuffer, sizeof(rxBuffer));
    
    if (state == RADIOLIB_ERR_NONE) {
      int len = radio.getPacketLength();
      int rssi = radio.getRSSI();
      float snr = radio.getSNR();
      
      packetsReceived++;
      
      // Print to Serial
      Serial.printf("\n📦 Packet #%lu\n", packetsReceived);
      Serial.printf("   Length: %d bytes\n", len);
      Serial.printf("   RSSI: %d dBm\n", rssi);
      Serial.printf("   SNR: %.2f dB\n", snr);
      
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
    Serial.printf("[Heartbeat] Running... RX: %lu\n", packetsReceived);
  }
  
  delay(10);
}
