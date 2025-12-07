/**
 * ============================================================================
 * LoRa Smart Wearable Monitoring System for Cruise Terminal Staff
 * with Integrated Dashboard and Analytics
 * ============================================================================
 * 
 * HELTEC VISION MASTER E290 - STAFF IDENTIFICATION BADGE
 * 
 * This device acts as a staff identification badge with emergency alert display
 * for cruise terminal operations monitoring.
 * 
 * Features:
 * - E-ink display showing staff name and real-time vitals
 * - Receives monitoring data from Wireless Stick V3 via UART
 * - Flashes emergency warnings for fall detection and unconscious states
 * - Low power consumption (E-ink only updates when needed)
 * 
 * Hardware:
 * - Heltec Vision Master E290 (ESP32-S3 with 2.9" E-ink display)
 * - UART connection to Wireless Stick V3 (TX/RX)
 * 
 * Communication Protocol (from Wireless Stick V3):
 * Realtime Packet (10 bytes):
 *   [0] Packet type: 0x01
 *   [1] Heart rate (BPM)
 *   [2] Body temperature
 *   [3] Ambient temperature
 *   [4] Noise level
 *   [5] Fall state (0=Normal, 1=Warning, 2=Fall, 3=Dangerous, 4=Recovery)
 *   [6] Alert flags
 *   [7-9] RSSI/SNR
 * 
 * ============================================================================
 */

#include <Arduino.h>
#include "HT_DEPG0290BxS800FxX_BW.h"

// ============================================================================
// PIN DEFINITIONS - Heltec Vision Master E290
// ============================================================================
// E-ink Display pins: rst, dc, cs, busy, sck, mosi, miso, frequency
DEPG0290BxS800FxX_BW display(5, 4, 3, 6, 2, 1, -1, 6000000);

// UART pins for communication with Wireless Stick V3
#define UART_RX     44  // Connect to Wireless Stick V3 TX
#define UART_TX     43  // Connect to Wireless Stick V3 RX
#define UART_BAUD   115200

// Button pins
#define BUTTON_PIN  0   // Built-in button for manual refresh
#define VEXT_PIN    18  // Power control pin

// ============================================================================
// CONFIGURATION
// ============================================================================
#define STAFF_NAME        "Yeung Wing"      // Staff member name
#define STAFF_TITLE       "Technician"
#define DEVICE_ID         "BADGE-001"

#define NORMAL_REFRESH_MS     30000        // Update every 30 seconds in normal mode
#define EMERGENCY_BLINK_MS    1000         // Blink every second in emergency
#define UART_TIMEOUT_MS       5000         // UART data timeout

/* Screen rotation options:
 * ANGLE_0_DEGREE
 * ANGLE_90_DEGREE
 * ANGLE_180_DEGREE
 * ANGLE_270_DEGREE
 */
#define DIRECTION ANGLE_0_DEGREE


// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
struct MonitoringData {
  uint8_t packet_type;
  uint8_t heart_rate;
  uint8_t body_temp;      // Changed from int8_t to uint8_t
  uint8_t ambient_temp;   // Changed from int8_t to uint8_t
  uint8_t noise_level;
  uint8_t fall_state;  // 0=Normal, 1=Warning, 2=Fall, 3=Dangerous, 4=Recovery
  uint8_t alert_flags;
  unsigned long last_update;
  bool valid;
};

MonitoringData currentData = {0};
unsigned long lastDisplayUpdate = 0;
unsigned long lastUARTReceive = 0;
bool emergencyMode = false;
bool displayState = false;  // For blinking in emergency mode

// ============================================================================
// POWER CONTROL
// ============================================================================
void VextON(void) {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH);
}

void VextOFF(void) {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
}

// ============================================================================
// UART COMMUNICATION
// ============================================================================

/**
 * Decode temperature from uint8 to float
 * Formula matches ESP32 encoding: (temp + 20) / 100 * 255
 * So decoding is: (encoded / 255) * 100 - 20
 */
float decodeTemperature(uint8_t encoded) {
  return ((float)encoded / 255.0f) * 100.0f - 20.0f;
}

/**
 * Read packet from UART
 * Returns true if valid packet received
 */
bool readUARTPacket() {
  if (Serial1.available() >= 10) {
    uint8_t buffer[10];
    
    // Read 10 bytes
    for (int i = 0; i < 10; i++) {
      buffer[i] = Serial1.read();
    }
    
    // Check packet type
    if (buffer[0] == 0x01) {  // Realtime data packet
      currentData.packet_type = buffer[0];
      currentData.heart_rate = buffer[1];
      currentData.body_temp = buffer[2];
      currentData.ambient_temp = buffer[3];
      currentData.noise_level = buffer[4];
      currentData.fall_state = buffer[5];
      currentData.alert_flags = buffer[6];
      currentData.last_update = millis();
      currentData.valid = true;
      lastUARTReceive = millis();
      
      Serial.printf("📦 Received: HR=%d, Temp_raw=%d (0x%02X), Temp_decoded=%.1f, Fall State=%d\n", 
                    currentData.heart_rate, 
                    (uint8_t)currentData.body_temp,
                    (uint8_t)currentData.body_temp,
                    decodeTemperature(currentData.body_temp),
                    currentData.fall_state);
      
      return true;
    }
  }
  
  return false;
}

/**
 * Check if data is stale
 */
bool isDataStale() {
  return (millis() - lastUARTReceive) > UART_TIMEOUT_MS;
}

// ============================================================================
// E-INK DISPLAY FUNCTIONS
// ============================================================================

/**
 * Display normal staff name tag
 */
void displayNormalMode() {
  display.clear();
  
  // Draw border
  display.drawRect(2, 2, display.width() - 4, display.height() - 4);
  display.drawRect(3, 3, display.width() - 6, display.height() - 6);
  
  // Staff name (large)
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(display.width() / 2, 20, STAFF_NAME);
  
  // Staff title
  display.setFont(ArialMT_Plain_16);
  display.drawString(display.width() / 2, 50, STAFF_TITLE);
  
  // Status indicator
  if (currentData.valid && !isDataStale()) {
    // Display vitals
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    char buffer[32];
    sprintf(buffer, "HR: %d BPM", currentData.heart_rate);
    display.drawString(10, 75, buffer);
    
    sprintf(buffer, "Temp: %.1fC", decodeTemperature(currentData.body_temp));
    display.drawString(10, 90, buffer);
    
    // Status text
    const char* status;
    switch(currentData.fall_state) {
      case 0: status = "Status: OK"; break;
      case 1: status = "Status: WARNING"; break;
      case 2: status = "Status: FALL!"; break;
      case 3: status = "Status: EMERGENCY!"; break;
      case 4: status = "Status: RECOVERY"; break;
      default: status = "Status: UNKNOWN"; break;
    }
    display.drawString(10, 105, status);
  } else {
    // No data / stale data
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.width() / 2, 85, "Waiting for sensor data...");
  }
  
  // Device ID
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(display.width() - 10, display.height() - 15, DEVICE_ID);
  
  display.display();
}

/**
 * Display emergency alert (Fall or Unconscious)
 */
void displayEmergencyMode(bool show) {
  display.clear();
  
  if (show) {
    // Fill screen (inverted for maximum visibility)
    display.fillRect(0, 0, display.width(), display.height());
    display.setColor(BLACK);
    
    // Warning text
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    
    const char* alertText;
    if (currentData.fall_state == 3) {
      alertText = "UNCONSCIOUS";
    } else {
      alertText = "FALL ALERT";
    }
    
    display.drawString(display.width() / 2, 10, alertText);
    
    // Staff name
    display.setFont(ArialMT_Plain_16);
    display.drawString(display.width() / 2, 45, STAFF_NAME);
    
    // Vitals
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    char buffer[32];
    sprintf(buffer, "HR: %d BPM", currentData.heart_rate);
    display.drawString(20, 70, buffer);
    
    sprintf(buffer, "%.1fC", decodeTemperature(currentData.body_temp));
    display.drawString(20, 85, buffer);
    
    // Alert message
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.width() / 2, 105, "IMMEDIATE ASSISTANCE");
    display.drawString(display.width() / 2, 115, "REQUIRED");
    
    display.setColor(WHITE);  // Reset color
  }
  
  display.display();
}

/**
 * Update display based on current state
 */
void updateDisplay() {
  // Check for emergency conditions
  if (currentData.valid && !isDataStale()) {
    if (currentData.fall_state == 2 || currentData.fall_state == 3) {
      // Emergency mode - blink display
      if (!emergencyMode) {
        emergencyMode = true;
        displayState = false;
        Serial.println("🚨 ENTERING EMERGENCY MODE!");
      }
      
      // Toggle display state for blinking
      displayState = !displayState;
      displayEmergencyMode(displayState);
      
    } else {
      // Normal mode
      if (emergencyMode) {
        emergencyMode = false;
        Serial.println("✅ Returning to normal mode");
      }
      displayNormalMode();
    }
    
  } else {
    // No data or stale data - show normal mode
    if (emergencyMode) {
      emergencyMode = false;
    }
    displayNormalMode();
  }
  
  lastDisplayUpdate = millis();
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  HELTEC VISION MASTER E290");
  Serial.println("  STAFF NAME TAG WITH EMERGENCY ALERT");
  Serial.println("========================================");
  Serial.printf("Staff Name: %s\n", STAFF_NAME);
  Serial.printf("Device ID: %s\n", DEVICE_ID);
  Serial.println("========================================\n");
  
  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Power on display
  VextON();
  delay(100);
  
  // Initialize UART communication with Wireless Stick V3
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  Serial.printf("UART initialized: RX=%d, TX=%d, Baud=%d\n", UART_RX, UART_TX, UART_BAUD);
  
  // Initialize E-ink display
  Serial.println("Initializing E-ink display...");
  display.init();
  display.screenRotate(DIRECTION);
  display.setFont(ArialMT_Plain_10);
  
  Serial.println("✅ E-ink display initialized!");
  
  // Display initial screen
  Serial.println("Displaying initial staff badge...");
  displayNormalMode();
  
  Serial.println("\n========================================");
  Serial.println("  SYSTEM READY");
  Serial.println("========================================");
  Serial.println("Waiting for data from Wireless Stick V3...\n");
  
  // Send dummy test packet after 5 seconds
  Serial.println("⏱️  Sending dummy test packet in 5 seconds...");
}

// ============================================================================
// DUMMY PACKET GENERATION (FOR TESTING)
// ============================================================================
void sendDummyPacket() {
  // Create dummy realtime packet (10 bytes)
  uint8_t dummyPacket[10];
  
  dummyPacket[0] = 0x01;  // Packet type: Realtime
  dummyPacket[1] = 75;    // Heart rate: 75 BPM
  dummyPacket[2] = 144;   // Body temp: 36.5°C encoded: (36.5+20)/100*255 = 144
  dummyPacket[3] = 102;   // Ambient temp: 20°C encoded: (20+20)/100*255 = 102
  dummyPacket[4] = 50;    // Noise level: Low
  dummyPacket[5] = 0;     // Fall state: Normal
  dummyPacket[6] = 0;     // Alert flags: None
  dummyPacket[7] = 200;   // RSSI (dummy)
  dummyPacket[8] = 100;   // SNR (dummy)
  dummyPacket[9] = 0;     // Reserved
  
  // Send via UART
  Serial1.write(dummyPacket, 10);
  
  Serial.println("📤 Dummy packet sent via UART:");
  Serial.printf("   HR=%d, Temp=%.1f°C, Fall State=%d\n", 
                dummyPacket[1], 
                decodeTemperature(dummyPacket[2]), 
                dummyPacket[5]);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  static bool dummyPacketSent = false;
  static unsigned long startTime = millis();
  
  // Send dummy packet after 5 seconds (once)
  if (!dummyPacketSent && (millis() - startTime >= 5000)) {
    sendDummyPacket();
    dummyPacketSent = true;
  }
  
  // Read UART data
  if (readUARTPacket()) {
    Serial.println("✅ Packet received and processed");
    // Immediately update display when new data arrives
    updateDisplay();
  }
  
  // Update display based on mode and timing
  unsigned long currentTime = millis();
  unsigned long updateInterval = emergencyMode ? EMERGENCY_BLINK_MS : NORMAL_REFRESH_MS;
  
  if (currentTime - lastDisplayUpdate >= updateInterval) {
    updateDisplay();
  }
  
  // Check for manual refresh button (send new dummy packet on press)
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("🔄 Button pressed - sending new dummy packet");
    delay(200);  // Debounce
    sendDummyPacket();
    updateDisplay();
    while(digitalRead(BUTTON_PIN) == LOW) delay(10);  // Wait for release
  }
  
  // Small delay to prevent CPU hogging
  delay(10);
}