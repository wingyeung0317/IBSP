#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>

/**
 * ==============================================================================
 * COMPREHENSIVE HEALTH MONITORING SYSTEM WITH WIFI
 * ==============================================================================
 * 
 * This implementation combines:
 * 1. Fall detection using MPU6050 accelerometer/gyroscope
 * 2. ECG/Heart rate monitoring using AD8232
 * 3. Body temperature monitoring using MLX90614
 * 4. Environmental noise monitoring using MAX4466 microphone
 * 5. WiFi data transmission via HTTPS to remote server
 * 
 * ==============================================================================
 * FALL DETECTION ALGORITHM IMPLEMENTATION FOR MPU6050
 * ==============================================================================
 * 
 * This implementation is based on research and established algorithms from:
 * 
 * Primary References:
 * 1. GitHub: xiaoweiweiyaya/ESP32_FallDetection
 *    https://github.com/xiaoweiweiyaya/ESP32_FallDetection
 *    - Algorithm: SVM (Signal Vector Magnitude) + Angular velocity + Posture angle
 *    - Post-fall monitoring: CV (Coefficient of Variation) + SD (Standard Deviation)
 *    
 * 2. GitHub: Kartik9250/Fall_detection
 *    https://github.com/Kartik9250/Fall_detection
 *    - Algorithm: Jerk-based detection (rate of change of acceleration)
 * 
 * Additional References:
 * 3. Academic: "Fall Detection Using Accelerometer and Gyroscope Data"
 *    - Common thresholds: Acceleration > 2-3g, Angular velocity > 200-300°/s
 * 
 * 4. Post-Fall Immobility Detection (based on xiaoweiweiyaya repository)
 *    - Monitors movement after fall detection
 *    - Uses variance and standard deviation to detect unconsciousness
 *    - Reference: components/posture/include/posture.h - FeatureData structure
 * 
 * Algorithm Overview:
 * -----------------
 * This implementation uses a multi-stage fall detection approach:
 * 
 * 1. JERK DETECTION (Rate of acceleration change)
 *    - Calculates jerk magnitude from acceleration changes
 *    - High jerk indicates sudden impact or movement
 * 
 * 2. SVM DETECTION (Signal Vector Magnitude)
 *    - Calculates total acceleration: sqrt(ax² + ay² + az²)
 *    - Detects both high-g impacts and low-g free-fall states
 * 
 * 3. ANGULAR VELOCITY CHECK
 *    - Monitors rotation speed during fall
 *    - High angular velocity indicates tumbling
 * 
 * 4. POSTURE ANGLE VERIFICATION
 *    - Checks pitch/roll angles after potential fall
 *    - Confirms if person is in a fallen position
 * 
 * 5. POST-FALL MOVEMENT MONITORING (NEW)
 *    - Monitors acceleration variance after fall detection
 *    - Detects immobility indicating possible unconsciousness
 *    - Triggers DANGEROUS state if no movement detected
 * 
 * All thresholds are configurable for different use cases and sensitivity needs.
 * ==============================================================================
 */

/**
 * MPU6050 6-Axis Motion Sensor Driver
 * 
 * This class provides a simple interface to read accelerometer and gyroscope
 * data from the MPU6050 sensor using I2C communication.
 */
class MPU6050 {
private:
  // MPU6050 I2C address and register definitions
  const uint8_t I2C_ADDR = 0x68;           // Default I2C address
  const uint8_t REG_PWR_MGMT_1 = 0x6B;     // Power management register
  const uint8_t REG_WHO_AM_I = 0x75;       // Device ID register
  const uint8_t REG_ACCEL_XOUT_H = 0x3B;   // Start of accelerometer data registers
  
  // Sensor calibration and scale factors
  const float ACCEL_SCALE = 16384.0;       // For ±2g range
  const float GYRO_SCALE = 131.0;          // For ±250°/s range
  const float GRAVITY = 9.80665;           // Standard gravity (m/s²)
  
  /**
   * Write a single byte to a specific MPU6050 register
   * @param reg Register address
   * @param data Data byte to write
   */
  void writeRegister(uint8_t reg, uint8_t data) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(data);
    Wire.endTransmission();
  }
  
  /**
   * Read a single byte from a specific MPU6050 register
   * @param reg Register address
   * @return Data byte read from register
   */
  uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.read();
  }

public:
  // Structure to hold sensor readings
  struct SensorData {
    float accelX, accelY, accelZ;  // Acceleration in m/s²
    float gyroX, gyroY, gyroZ;     // Rotation rate in °/s
  };
  
  /**
   * Initialize the MPU6050 sensor
   * Wake up the sensor from sleep mode and verify communication
   * @return true if initialization successful, false otherwise
   */
  bool begin() {
    // Wake up MPU6050 (it starts in sleep mode by default)
    writeRegister(REG_PWR_MGMT_1, 0x00);
    delay(100);
    
    // Verify device identity by reading WHO_AM_I register
    uint8_t deviceId = readRegister(REG_WHO_AM_I);
    
    // Print device ID for debugging
    Serial.print("Device ID: 0x");
    Serial.print(deviceId, HEX);
    
    // Valid device IDs:
    // 0x68 - MPU6050
    // 0x70 - MPU6500
    // 0x71 - MPU9250
    // 0x73 - MPU9255
    // 0x98 - MPU6050 (alternate)
    if (deviceId == 0x68 || deviceId == 0x70 || deviceId == 0x71 || 
        deviceId == 0x73 || deviceId == 0x98) {
      
      // Identify the specific sensor model
      String sensorName;
      switch(deviceId) {
        case 0x68: sensorName = "MPU6050"; break;
        case 0x70: sensorName = "MPU6500"; break;
        case 0x71: sensorName = "MPU9250"; break;
        case 0x73: sensorName = "MPU9255"; break;
        case 0x98: sensorName = "MPU6050 (alt)"; break;
        default: sensorName = "Unknown"; break;
      }
      
      Serial.print(" - ");
      Serial.print(sensorName);
      Serial.println(" detected!");
      Serial.println("Sensor initialized successfully!");
      return true;
    } else {
      Serial.println(" - Unknown/Unsupported device");
      Serial.println("This device ID is not recognized.");
      Serial.println("The sensor may still work, attempting to continue...");
      // Try to continue anyway - some clones use different IDs
      return true;
    }
  }
  
  /**
   * Read all sensor data from MPU6050
   * Reads 14 bytes of data containing accelerometer, temperature, and gyroscope
   * @return SensorData structure with converted physical values
   */
  SensorData readSensorData() {
    SensorData data;
    int16_t rawData[7];  // ax, ay, az, temp, gx, gy, gz
    
    // Request 14 bytes starting from ACCEL_XOUT_H register
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)14);
    
    // Read all 14 bytes (7 x 16-bit values)
    for (int i = 0; i < 7; i++) {
      rawData[i] = (Wire.read() << 8) | Wire.read();
    }
    
    // Convert raw values to physical units
    data.accelX = (rawData[0] / ACCEL_SCALE) * GRAVITY;  // m/s²
    data.accelY = (rawData[1] / ACCEL_SCALE) * GRAVITY;
    data.accelZ = (rawData[2] / ACCEL_SCALE) * GRAVITY;
    
    data.gyroX = rawData[4] / GYRO_SCALE;  // °/s
    data.gyroY = rawData[5] / GYRO_SCALE;
    data.gyroZ = rawData[6] / GYRO_SCALE;
    
    return data;
  }
  
  /**
   * Print formatted sensor data to Serial monitor
   * @param data SensorData structure to display
   */
  void printData(const SensorData& data) {
    Serial.println("========================================");
    
    // Acceleration data
    Serial.print("Acceleration (m/s²): X=");
    Serial.print(data.accelX, 3);
    Serial.print(" Y=");
    Serial.print(data.accelY, 3);
    Serial.print(" Z=");
    Serial.println(data.accelZ, 3);
    
    // Gyroscope data
    Serial.print("Gyroscope (°/s):     X=");
    Serial.print(data.gyroX, 2);
    Serial.print(" Y=");
    Serial.print(data.gyroY, 2);
    Serial.print(" Z=");
    Serial.println(data.gyroZ, 2);
    
    Serial.println();
  }
};

// ============================================================================
// FALL DETECTION ALGORITHM CLASS
// ============================================================================

/**
 * FallDetector - Advanced fall detection using MPU6050 sensor
 * 
 * This class implements a multi-criteria fall detection algorithm that can be
 * easily tuned for different applications (elderly care, sports, workplace safety).
 * 
 * Configurable Parameters:
 * - All threshold values are public and can be adjusted at runtime
 * - Allows for AI/ML optimization of parameters based on user data
 * - Supports different sensitivity profiles (conservative, balanced, sensitive)
 */
class FallDetector {
public:
  // ===========================================================================
  // CONFIGURABLE THRESHOLD PARAMETERS
  // ===========================================================================
  // These can be adjusted based on user needs, environment, or AI training
  
  // --- Jerk Detection Thresholds (m/s³) ---
  // Optimized for TORSO/CENTER-BODY placement - lower impact forces expected
  // Reference: Kartik9250/Fall_detection uses 650,000 m/s³ for wrist placement
  float JERK_THRESHOLD_HIGH = 450000.0f;     // Reduced for torso (was 650,000)
  float JERK_THRESHOLD_MEDIUM = 300000.0f;   // Reduced for torso (was 400,000)
  float JERK_THRESHOLD_LOW = 200000.0f;      // NEW: Detect subtle changes
  
  // --- SVM (Signal Vector Magnitude) Thresholds (g-force) ---
  // Torso experiences less extreme values than extremities
  // Normal gravity = 1g, torso fall impact = 1.5-2.5g (lower than wrist/head)
  float SVM_THRESHOLD_HIGH = 1.8f;      // Reduced for torso (was 2.5g)
  float SVM_THRESHOLD_LOW = 0.65f;      // Slightly raised to avoid bowing detection
  float SVM_THRESHOLD_WARNING = 1.4f;   // Reduced for torso (was 1.8g)
  float SVM_THRESHOLD_IMPACT_PEAK = 2.2f; // NEW: Very high impact (confirms fall)
  
  // --- Angular Velocity Thresholds (°/s) ---
  // Torso rotation is KEY indicator - body tumbles during fall
  float GYRO_THRESHOLD = 150.0f;        // Reduced - torso rotates slower (was 200)
  float GYRO_THRESHOLD_COMBINED = 180.0f; // Reduced for torso (was 250)
  float GYRO_THRESHOLD_SUSTAINED = 120.0f; // NEW: Sustained rotation detection
  
  // --- Posture Angle Thresholds (degrees) ---
  // Critical for torso placement - body orientation changes significantly
  float PITCH_THRESHOLD = 40.0f;        // Slightly reduced (was 45°)
  float ROLL_THRESHOLD = 35.0f;         // Increased importance (was 30°)
  float POSTURE_CHANGE_RAPID = 60.0f;   // NEW: Rapid angle change indicates fall
  
  // --- Time Windows (milliseconds) ---
  uint32_t FALL_CONFIRMATION_WINDOW = 500;   // Initial wait before starting immobility check
  uint32_t RECOVERY_TIME_WINDOW = 5000;      // Minimum time between fall detections
  uint32_t JERK_SAMPLING_INTERVAL = 10;      // Interval for jerk calculation
  uint32_t IMMOBILITY_CHECK_WINDOW = 3000;   // Time window to monitor for movement after fall
  uint32_t IMMOBILITY_SAMPLING_INTERVAL = 100; // Interval for immobility sampling
  uint32_t FALL_SEQUENCE_WINDOW = 800;       // NEW: Time window to detect fall sequence
  uint32_t BOWING_REJECTION_TIME = 1500;     // NEW: Bowing takes longer than falling
  
  // --- Detection Stage Counters ---
  uint8_t IMPACT_COUNT_THRESHOLD = 2;    // Number of high-g readings to trigger
  uint8_t WARNING_COUNT_THRESHOLD = 3;   // Number of warning readings
  uint8_t GYRO_SUSTAINED_COUNT = 3;      // NEW: Sustained rotation counter
  
  // --- Post-Fall Movement Detection Thresholds ---
  // Reference: xiaoweiweiyaya/ESP32_FallDetection - uses CV and SD for movement analysis
  float IMMOBILITY_ACCEL_VARIANCE_THRESHOLD = 0.005f;  // Max variance (m/s²)² for immobile state
  float IMMOBILITY_ACCEL_STDDEV_THRESHOLD = 0.1f;    // Max standard deviation (m/s²) for immobile
  float IMMOBILITY_GYRO_VARIANCE_THRESHOLD = 5.0f;    // Max gyro variance (°/s)² for immobile
  float IMMOBILITY_SVM_RANGE_THRESHOLD = 0.1f;        // Max SVM change for immobile (g)
  uint8_t IMMOBILITY_SAMPLE_COUNT = 10;               // Number of samples to check for immobility
  
  // ===========================================================================
  // FALL DETECTION STATE
  // ===========================================================================
  
  enum FallState {
    NORMAL = 0,        // Normal activity
    WARNING = 1,       // Potential fall detected, monitoring
    FALL_DETECTED = 2, // Confirmed fall
    DANGEROUS = 3,     // Immobile after fall - possible unconsciousness
    RECOVERY = 4       // Post-fall recovery period
  };
  
  struct FallEvent {
    FallState state;
    uint32_t timestamp;
    float jerk_magnitude;
    float svm_value;
    float angular_velocity;
    float pitch_angle;
    float roll_angle;
    bool confirmed;
    
    // Post-fall immobility metrics
    float movement_variance;      // Acceleration variance after fall
    float movement_stddev;        // Standard deviation of movement
    bool is_immobile;            // True if person appears unconscious/immobile
    uint32_t immobile_duration;  // How long person has been immobile (ms)
  };
  
private:
  // Previous sensor readings for derivative calculations
  float prev_accel_x, prev_accel_y, prev_accel_z;
  float baseline_pitch, baseline_roll;
  
  // Detection state tracking
  FallState current_state;
  uint32_t state_change_time;
  uint32_t last_fall_time;
  uint8_t impact_counter;
  uint8_t warning_counter;
  uint8_t gyro_sustained_counter;  // NEW: Track sustained rotation
  
  // Sample timing
  uint32_t last_sample_time;
  uint32_t last_immobility_check_time;
  uint32_t warning_start_time;     // NEW: Track when warning state started
  
  // Fall sequence detection
  bool detected_freefall;          // NEW: Track if free-fall detected
  bool detected_impact;            // NEW: Track if impact detected
  bool detected_rotation;          // NEW: Track if rotation detected
  uint32_t freefall_time;          // NEW: When free-fall was detected
  uint32_t impact_time;            // NEW: When impact was detected
  
  // Activity pattern tracking (for bowing/jumping rejection)
  float prev_svm;                  // NEW: Previous SVM value
  float svm_trend;                 // NEW: SVM trend (increasing/decreasing)
  uint8_t vertical_motion_counter; // NEW: Count vertical motions (jumping)
  
  // Latest fall event data
  FallEvent latest_event;
  
  // Calibration flag
  bool is_calibrated;
  
  // Post-fall immobility monitoring
  struct ImmobilityBuffer {
    float accel_samples[30];  // Circular buffer for acceleration samples
    float gyro_samples[30];   // Circular buffer for gyro samples
    float svm_samples[30];    // Circular buffer for SVM samples
    uint8_t sample_index;
    uint8_t sample_count;
  } immobility_buffer;
  
  uint32_t immobility_start_time;
  
public:
  /**
   * Constructor - Initialize fall detector
   */
  FallDetector() {
    prev_accel_x = 0.0f;
    prev_accel_y = 0.0f;
    prev_accel_z = 0.0f;
    baseline_pitch = 0.0f;
    baseline_roll = 0.0f;
    
    current_state = NORMAL;
    state_change_time = 0;
    last_fall_time = 0;
    impact_counter = 0;
    warning_counter = 0;
    gyro_sustained_counter = 0;
    
    last_sample_time = 0;
    last_immobility_check_time = 0;
    warning_start_time = 0;
    is_calibrated = false;
    
    // Fall sequence tracking
    detected_freefall = false;
    detected_impact = false;
    detected_rotation = false;
    freefall_time = 0;
    impact_time = 0;
    
    // Activity pattern tracking
    prev_svm = 1.0f;
    svm_trend = 0.0f;
    vertical_motion_counter = 0;
    
    latest_event.state = NORMAL;
    latest_event.confirmed = false;
    latest_event.is_immobile = false;
    latest_event.movement_variance = 0.0f;
    latest_event.movement_stddev = 0.0f;
    latest_event.immobile_duration = 0;
    
    // Initialize immobility buffer
    immobility_buffer.sample_index = 0;
    immobility_buffer.sample_count = 0;
    for (int i = 0; i < 30; i++) {
      immobility_buffer.accel_samples[i] = 0.0f;
      immobility_buffer.gyro_samples[i] = 0.0f;
      immobility_buffer.svm_samples[i] = 1.0f;  // Initialize to 1g
    }
    immobility_start_time = 0;
  }
  
  /**
   * Calibrate baseline posture angles
   * Should be called when user is in normal standing/sitting position
   * 
   * @param pitch Current pitch angle
   * @param roll Current roll angle
   */
  void calibrate(float pitch, float roll) {
    baseline_pitch = pitch;
    baseline_roll = roll;
    is_calibrated = true;
    
    Serial.println("========================================");
    Serial.println("Fall Detector Calibrated!");
    Serial.print("Baseline Pitch: ");
    Serial.print(baseline_pitch, 2);
    Serial.print("°, Baseline Roll: ");
    Serial.print(baseline_roll, 2);
    Serial.println("°");
    Serial.println("========================================\n");
  }
  
  /**
   * Calculate jerk magnitude (rate of change of acceleration)
   * 
   * @param accel_x Current X acceleration (m/s²)
   * @param accel_y Current Y acceleration (m/s²)
   * @param accel_z Current Z acceleration (m/s²)
   * @param delta_time Time since last sample (seconds)
   * @return Jerk magnitude (m/s³)
   */
  float calculateJerk(float accel_x, float accel_y, float accel_z, float delta_time) {
    // Calculate jerk components (derivative of acceleration)
    float jerk_x = (accel_x - prev_accel_x) / delta_time;
    float jerk_y = (accel_y - prev_accel_y) / delta_time;
    float jerk_z = (accel_z - prev_accel_z) / delta_time;
    
    // Update previous values
    prev_accel_x = accel_x;
    prev_accel_y = accel_y;
    prev_accel_z = accel_z;
    
    // Calculate magnitude
    float jerk_magnitude = sqrt(jerk_x * jerk_x + jerk_y * jerk_y + jerk_z * jerk_z);
    
    return jerk_magnitude;
  }
  
  /**
   * Calculate Signal Vector Magnitude (SVM)
   * Represents total acceleration magnitude
   * 
   * @param accel_x X acceleration (m/s²)
   * @param accel_y Y acceleration (m/s²)
   * @param accel_z Z acceleration (m/s²)
   * @return SVM in g-force units
   */
  float calculateSVM(float accel_x, float accel_y, float accel_z) {
    // Convert m/s² to g (1g = 9.80665 m/s²)
    const float GRAVITY = 9.80665f;
    float ax_g = accel_x / GRAVITY;
    float ay_g = accel_y / GRAVITY;
    float az_g = accel_z / GRAVITY;
    
    // Calculate magnitude
    float svm = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
    
    return svm;
  }
  
  /**
   * Calculate combined angular velocity magnitude
   * 
   * @param gyro_x X rotation rate (°/s)
   * @param gyro_y Y rotation rate (°/s)
   * @param gyro_z Z rotation rate (°/s)
   * @return Combined angular velocity magnitude
   */
  float calculateAngularVelocity(float gyro_x, float gyro_y, float gyro_z) {
    return sqrt(gyro_x * gyro_x + gyro_y * gyro_y + gyro_z * gyro_z);
  }
  
  /**
   * Main fall detection algorithm
   * Analyzes sensor data using multi-stage criteria
   * 
   * Algorithm Flow:
   * 1. Calculate jerk, SVM, and angular velocity
   * 2. Check for impact/free-fall phase (Stage 1)
   * 3. Check for tumbling/rotation (Stage 2)
   * 4. Verify posture angle change (Stage 3)
   * 5. Confirm fall if all criteria met
   * 
   * @param sensor_data Current MPU6050 sensor readings
   * @return Updated fall event with detection status
   */
  FallEvent detectFall(const MPU6050::SensorData& sensor_data) {
    uint32_t current_time = millis();
    
    // Calculate time delta for jerk calculation
    float delta_time = (current_time - last_sample_time) / 1000.0f; // Convert to seconds
    if (delta_time <= 0) delta_time = 0.01f; // Prevent division by zero
    last_sample_time = current_time;
    
    // === STAGE 1: Calculate Detection Metrics ===
    
    float jerk_mag = calculateJerk(sensor_data.accelX, sensor_data.accelY, 
                                   sensor_data.accelZ, delta_time);
    
    float svm = calculateSVM(sensor_data.accelX, sensor_data.accelY, 
                            sensor_data.accelZ);
    
    float angular_vel = calculateAngularVelocity(sensor_data.gyroX, sensor_data.gyroY, 
                                                 sensor_data.gyroZ);
    
    // === STAGE 2: Enhanced Impact/Free-fall Detection ===
    // Optimized for TORSO placement - detect fall SEQUENCE instead of single event
    
    // Calculate SVM trend for pattern recognition
    svm_trend = svm - prev_svm;
    prev_svm = svm;
    
    bool high_impact = (svm > SVM_THRESHOLD_HIGH);
    bool very_high_impact = (svm > SVM_THRESHOLD_IMPACT_PEAK);
    bool free_fall = (svm < SVM_THRESHOLD_LOW);
    bool high_jerk = (jerk_mag > JERK_THRESHOLD_HIGH);
    bool medium_jerk = (jerk_mag > JERK_THRESHOLD_MEDIUM);
    bool low_jerk = (jerk_mag > JERK_THRESHOLD_LOW);
    
    // NEW: Detect free-fall phase (important for fall sequence)
    if (free_fall && !detected_freefall) {
      detected_freefall = true;
      freefall_time = current_time;
      Serial.println("[Fall Sequence] Free-fall detected!");
    }
    
    // Reset free-fall if too much time passed
    if (detected_freefall && (current_time - freefall_time > FALL_SEQUENCE_WINDOW)) {
      detected_freefall = false;
    }
    
    // NEW: Detect impact phase (especially after free-fall)
    if ((high_impact || high_jerk) && !detected_impact) {
      detected_impact = true;
      impact_time = current_time;
      
      // Extra confidence if impact follows free-fall
      if (detected_freefall && (current_time - freefall_time < FALL_SEQUENCE_WINDOW)) {
        Serial.println("[Fall Sequence] Impact after free-fall - HIGH CONFIDENCE!");
        impact_counter += 2;  // Double weight for sequence detection
      } else {
        impact_counter++;
      }
      
      if (current_state == NORMAL) {
        current_state = WARNING;
        warning_start_time = current_time;
      }
    }
    
    // Reset impact detection
    if (detected_impact && (current_time - impact_time > FALL_SEQUENCE_WINDOW)) {
      detected_impact = false;
    }
    
    // Additional impact detection for torso
    if (very_high_impact) {
      impact_counter += 2;  // Strong signal
      current_state = WARNING;
      if (warning_start_time == 0) warning_start_time = current_time;
    } else if (medium_jerk || (svm > SVM_THRESHOLD_WARNING)) {
      warning_counter++;
      if (warning_counter >= WARNING_COUNT_THRESHOLD) {
        current_state = WARNING;
        if (warning_start_time == 0) warning_start_time = current_time;
      }
    } else if (low_jerk && current_state == WARNING) {
      // Keep warning state active with low jerk
      warning_counter++;
    } else {
      // Decay counters if no detection
      if (impact_counter > 0) impact_counter--;
      if (warning_counter > 0) warning_counter--;
      
      // Return to normal if no activity
      if (current_state == WARNING && impact_counter == 0 && warning_counter == 0) {
        current_state = NORMAL;
        warning_start_time = 0;
      }
    }
    
    // === STAGE 3: Enhanced Rotation/Tumbling Detection ===
    // Torso rotation is CRITICAL - body tumbles during fall but not during bowing
    bool high_rotation = (angular_vel > GYRO_THRESHOLD_COMBINED);
    bool sustained_rotation = (angular_vel > GYRO_THRESHOLD_SUSTAINED);
    
    // Track sustained rotation (key difference: fall = rotation, bowing = no rotation)
    if (sustained_rotation) {
      gyro_sustained_counter++;
      detected_rotation = true;
      Serial.print("[Rotation] Sustained rotation detected: ");
      Serial.print(angular_vel);
      Serial.print(" °/s (count: ");
      Serial.print(gyro_sustained_counter);
      Serial.println(")");
    } else {
      if (gyro_sustained_counter > 0) gyro_sustained_counter--;
    }
    
    // High confidence if sustained rotation during warning state
    if (current_state == WARNING && gyro_sustained_counter >= GYRO_SUSTAINED_COUNT) {
      Serial.println("[Rotation] Sustained rotation confirmed - likely fall!");
    }
    
    // === STAGE 4: Enhanced Posture Angle Verification ===
    // Torso orientation is HIGHLY reliable indicator
    bool posture_changed = false;
    bool rapid_posture_change = false;
    static float prev_pitch = 0;
    static float prev_roll = 0;
    
    if (is_calibrated) {
      // Calculate pitch and roll from accelerometer
      float pitch = atan2(sensor_data.accelY, sensor_data.accelZ) * 180.0f / PI;
      float roll = atan2(sensor_data.accelX, sensor_data.accelZ) * 180.0f / PI;
      
      // Check if posture significantly changed from baseline
      float pitch_change = abs(pitch - baseline_pitch);
      float roll_change = abs(roll - baseline_roll);
      
      // NEW: Calculate rate of angle change (fast = fall, slow = bowing)
      float pitch_rate = abs(pitch - prev_pitch) / (delta_time + 0.001f);
      float roll_rate = abs(roll - prev_roll) / (delta_time + 0.001f);
      
      prev_pitch = pitch;
      prev_roll = roll;
      
      posture_changed = (pitch_change > PITCH_THRESHOLD) || (roll_change > ROLL_THRESHOLD);
      
      // NEW: Rapid posture change indicates fall (not slow bowing)
      rapid_posture_change = (pitch_change > POSTURE_CHANGE_RAPID) || 
                             (roll_change > POSTURE_CHANGE_RAPID) ||
                             (pitch_rate > 100.0f) || (roll_rate > 80.0f);
      
      if (rapid_posture_change && current_state == WARNING) {
        Serial.println("[Posture] RAPID angle change detected - strong fall indicator!");
      }
      
      latest_event.pitch_angle = pitch;
      latest_event.roll_angle = roll;
    }
    
    // === STAGE 5: Intelligent Fall Confirmation Logic ===
    // ENHANCED: Distinguish fall from bowing/jumping using multiple indicators
    
    bool fall_confirmed = false;
    
    if (current_state == WARNING) {
      uint32_t warning_duration = current_time - warning_start_time;
      
      // === BOWING REJECTION ===
      // Bowing characteristics: slow, controlled, no rotation, gradual angle change
      bool likely_bowing = false;
      if (is_calibrated) {
        // Bowing takes longer (>1.5s), has no rotation, and is controlled
        likely_bowing = (warning_duration > BOWING_REJECTION_TIME) && 
                       (gyro_sustained_counter == 0) && 
                       (!detected_freefall) &&
                       (!rapid_posture_change);
        
        if (likely_bowing) {
          Serial.println("[Rejection] Likely BOWING detected - slow, no rotation");
          // Reset warning state
          current_state = NORMAL;
          impact_counter = 0;
          warning_counter = 0;
          gyro_sustained_counter = 0;
          warning_start_time = 0;
          detected_freefall = false;
          detected_impact = false;
          detected_rotation = false;
          return latest_event;
        }
      }
      
      // === JUMPING REJECTION ===
      // Jumping characteristics: vertical motion, symmetric up/down, quick recovery
      bool likely_jumping = false;
      if (detected_freefall && !detected_rotation && (gyro_sustained_counter == 0)) {
        // Jump has free-fall but NO rotation and quick posture return
        likely_jumping = true;
        Serial.println("[Rejection] Likely JUMPING detected - vertical, no rotation");
        
        // Don't immediately reject - wait to see if posture changes
        // If person returns to upright quickly, it was a jump
      }
      
      // === FALL CONFIRMATION WITH WEIGHTED CRITERIA ===
      int criteria_score = 0;
      int criteria_count = 0;
      
      // Criterion 1: Impact detection (weight: 2 if after free-fall, else 1)
      if (impact_counter >= IMPACT_COUNT_THRESHOLD) {
        criteria_count++;
        if (detected_freefall && (impact_time - freefall_time < FALL_SEQUENCE_WINDOW)) {
          criteria_score += 3;  // STRONG indicator: free-fall → impact sequence
          Serial.println("[Criteria] ✓ Impact sequence (score +3)");
        } else {
          criteria_score += 1;
          Serial.println("[Criteria] ✓ Impact detected (score +1)");
        }
      }
      
      // Criterion 2: Sustained rotation (weight: 3 - CRITICAL for torso)
      if (gyro_sustained_counter >= GYRO_SUSTAINED_COUNT) {
        criteria_count++;
        criteria_score += 3;  // STRONG indicator: body tumbling
        Serial.println("[Criteria] ✓ Sustained rotation (score +3)");
      } else if (high_rotation) {
        criteria_count++;
        criteria_score += 2;
        Serial.println("[Criteria] ✓ High rotation (score +2)");
      }
      
      // Criterion 3: Posture change (weight: 3 if rapid, else 2)
      if (rapid_posture_change) {
        criteria_count++;
        criteria_score += 3;  // STRONG indicator: sudden orientation change
        Serial.println("[Criteria] ✓ Rapid posture change (score +3)");
      } else if (posture_changed) {
        criteria_count++;
        criteria_score += 2;
        Serial.println("[Criteria] ✓ Posture changed (score +2)");
      }
      
      // Criterion 4: High jerk (weight: 1)
      if (high_jerk) {
        criteria_count++;
        criteria_score += 1;
        Serial.println("[Criteria] ✓ High jerk (score +1)");
      }
      
      // Criterion 5: Fall sequence detected (weight: 2)
      if (detected_freefall && detected_impact && detected_rotation) {
        criteria_count++;
        criteria_score += 2;  // Complete fall sequence
        Serial.println("[Criteria] ✓ Complete fall sequence (score +2)");
      }
      
      Serial.print("[Fall Score] Total: ");
      Serial.print(criteria_score);
      Serial.print("/12, Criteria: ");
      Serial.print(criteria_count);
      Serial.println("/5");
      
      // === CONFIRMATION DECISION ===
      // Require EITHER:
      // - Score ≥ 6 (high confidence)
      // - Score ≥ 4 AND at least 3 different criteria
      // This ensures we don't miss falls but avoid false positives
      
      if ((criteria_score >= 6) || (criteria_score >= 4 && criteria_count >= 3)) {
        if (!likely_jumping) {
          fall_confirmed = true;
          current_state = FALL_DETECTED;
          state_change_time = current_time;
          last_fall_time = current_time;
          
          Serial.println("\n╔══════════════════════════════════════╗");
          Serial.println("║     ⚠️  FALL CONFIRMED!  ⚠️         ║");
          Serial.println("╚══════════════════════════════════════╝");
          Serial.print("Score: ");
          Serial.print(criteria_score);
          Serial.print(", Criteria: ");
          Serial.println(criteria_count);
          
          // Reset counters
          impact_counter = 0;
          warning_counter = 0;
          gyro_sustained_counter = 0;
          detected_freefall = false;
          detected_impact = false;
          detected_rotation = false;
        } else {
          Serial.println("[Decision] High score but likely jumping - monitoring...");
        }
      } else if (warning_duration > FALL_SEQUENCE_WINDOW && criteria_score < 4) {
        // Timeout - not enough evidence for fall
        Serial.println("[Decision] Warning timeout - insufficient evidence");
        current_state = NORMAL;
        impact_counter = 0;
        warning_counter = 0;
        gyro_sustained_counter = 0;
        warning_start_time = 0;
        detected_freefall = false;
        detected_impact = false;
        detected_rotation = false;
      }
    }
    
    // === STAGE 6: Post-Fall Movement Monitoring ===
    // Monitor for immobility after fall detection (potential unconsciousness)
    if (current_state == FALL_DETECTED) {
      // Check if it's time to start monitoring immobility
      if (current_time - state_change_time > FALL_CONFIRMATION_WINDOW) {
        // Continuously collect samples
        checkPostFallMovement(svm, angular_vel, current_time);
        
        // Only make decision after collecting enough samples
        if (immobility_buffer.sample_count >= IMMOBILITY_SAMPLE_COUNT) {
          // Check if person is immobile
          if (latest_event.is_immobile) {
            current_state = DANGEROUS;
            Serial.println("\n!!! WARNING: NO MOVEMENT DETECTED - POSSIBLE UNCONSCIOUSNESS !!!");
          } else {
            // Person is moving, transition to recovery
            current_state = RECOVERY;
            Serial.println("Movement detected - person is moving after fall");
          }
        }
        // If not enough samples yet, stay in FALL_DETECTED state
      }
    }
    
    // === STAGE 7: Dangerous State - Continuous Immobility Monitoring ===
    if (current_state == DANGEROUS) {
      // Continue monitoring movement
      checkPostFallMovement(svm, angular_vel, current_time);
      
      // If person starts moving, transition to recovery
      if (!latest_event.is_immobile) {
        Serial.println("Movement detected - transitioning to recovery");
        current_state = RECOVERY;
      }
    }
    
    // === STAGE 8: Recovery Period ===
    // Prevent multiple detections in short time
    if (current_state == RECOVERY) {
      if (current_time - last_fall_time > RECOVERY_TIME_WINDOW) {
        current_state = NORMAL;
        // Reset immobility tracking
        immobility_buffer.sample_count = 0;
        immobility_buffer.sample_index = 0;
      }
    }
    
    // Update fall event data
    latest_event.state = current_state;
    latest_event.timestamp = current_time;
    latest_event.jerk_magnitude = jerk_mag;
    latest_event.svm_value = svm;
    latest_event.angular_velocity = angular_vel;
    latest_event.confirmed = fall_confirmed;
    
    return latest_event;
  }
  
  /**
   * Check for post-fall movement to detect unconsciousness
   * 
   * This method monitors acceleration variance and standard deviation after a fall
   * to determine if the person is immobile (potentially unconscious/injured).
   * 
   * Algorithm based on xiaoweiweiyaya/ESP32_FallDetection approach:
   * - Collects acceleration samples in a sliding window
   * - Calculates variance and standard deviation
   * - Low variance indicates no movement (immobility)
   * - High variance indicates normal movement
   * 
   * @param svm Current Signal Vector Magnitude
   * @param angular_vel Current angular velocity magnitude
   * @param current_time Current timestamp in milliseconds
   */
  void checkPostFallMovement(float svm, float angular_vel, uint32_t current_time) {
    // Only check at specified intervals to avoid excessive computation
    if (current_time - last_immobility_check_time < IMMOBILITY_SAMPLING_INTERVAL) {
      return;
    }
    last_immobility_check_time = current_time;
    
    // Add current samples to circular buffer
    immobility_buffer.accel_samples[immobility_buffer.sample_index] = svm;
    immobility_buffer.gyro_samples[immobility_buffer.sample_index] = angular_vel;
    immobility_buffer.svm_samples[immobility_buffer.sample_index] = svm;
    
    // Debug: Print sample collection
    Serial.print("[Immobility] Sample ");
    Serial.print(immobility_buffer.sample_count);
    Serial.print("/");
    Serial.print(IMMOBILITY_SAMPLE_COUNT);
    Serial.print(" - SVM: ");
    Serial.print(svm, 3);
    Serial.print(", Gyro: ");
    Serial.println(angular_vel, 2);
    
    immobility_buffer.sample_index++;
    if (immobility_buffer.sample_index >= IMMOBILITY_SAMPLE_COUNT) {
      immobility_buffer.sample_index = 0;
    }
    
    if (immobility_buffer.sample_count < IMMOBILITY_SAMPLE_COUNT) {
      immobility_buffer.sample_count++;
    }
    
    // Need minimum samples before checking
    if (immobility_buffer.sample_count < IMMOBILITY_SAMPLE_COUNT) {
      latest_event.is_immobile = false;
      return;
    }
    
    // Calculate variance and standard deviation of acceleration
    float accel_mean = 0.0f;
    float gyro_mean = 0.0f;
    float svm_min = 999.0f;
    float svm_max = 0.0f;
    
    // Calculate means and range
    for (int i = 0; i < IMMOBILITY_SAMPLE_COUNT; i++) {
      accel_mean += immobility_buffer.accel_samples[i];
      gyro_mean += immobility_buffer.gyro_samples[i];
      
      if (immobility_buffer.svm_samples[i] < svm_min) svm_min = immobility_buffer.svm_samples[i];
      if (immobility_buffer.svm_samples[i] > svm_max) svm_max = immobility_buffer.svm_samples[i];
    }
    accel_mean /= IMMOBILITY_SAMPLE_COUNT;
    gyro_mean /= IMMOBILITY_SAMPLE_COUNT;
    
    float svm_range = svm_max - svm_min;
    
    // Calculate variance
    float accel_variance = 0.0f;
    float gyro_variance = 0.0f;
    
    for (int i = 0; i < IMMOBILITY_SAMPLE_COUNT; i++) {
      float accel_diff = immobility_buffer.accel_samples[i] - accel_mean;
      float gyro_diff = immobility_buffer.gyro_samples[i] - gyro_mean;
      
      accel_variance += accel_diff * accel_diff;
      gyro_variance += gyro_diff * gyro_diff;
    }
    accel_variance /= IMMOBILITY_SAMPLE_COUNT;
    gyro_variance /= IMMOBILITY_SAMPLE_COUNT;
    
    // Calculate standard deviation
    float accel_stddev = sqrt(accel_variance);
    
    // Update event metrics
    latest_event.movement_variance = accel_variance;
    latest_event.movement_stddev = accel_stddev;
    
    // Debug: Print calculated metrics
    Serial.println("\n[Immobility Analysis]");
    Serial.print("  Accel Mean: "); Serial.print(accel_mean, 3); Serial.println(" g");
    Serial.print("  Accel Variance: "); Serial.print(accel_variance, 6); Serial.print(" (threshold: ");
    Serial.print(IMMOBILITY_ACCEL_VARIANCE_THRESHOLD, 6); Serial.println(")");
    Serial.print("  Accel StdDev: "); Serial.print(accel_stddev, 4); Serial.print(" (threshold: ");
    Serial.print(IMMOBILITY_ACCEL_STDDEV_THRESHOLD, 4); Serial.println(")");
    Serial.print("  Gyro Variance: "); Serial.print(gyro_variance, 2); Serial.print(" (threshold: ");
    Serial.print(IMMOBILITY_GYRO_VARIANCE_THRESHOLD, 2); Serial.println(")");
    Serial.print("  SVM Range: "); Serial.print(svm_range, 3); Serial.print(" (threshold: ");
    Serial.print(IMMOBILITY_SVM_RANGE_THRESHOLD, 3); Serial.println(")");
    
    // Determine immobility based on multiple criteria
    // Reference: xiaoweiweiyaya uses CV and SD thresholds
    bool low_accel_variance = (accel_variance < IMMOBILITY_ACCEL_VARIANCE_THRESHOLD);
    bool low_accel_stddev = (accel_stddev < IMMOBILITY_ACCEL_STDDEV_THRESHOLD);
    bool low_gyro_variance = (gyro_variance < IMMOBILITY_GYRO_VARIANCE_THRESHOLD);
    bool low_svm_range = (svm_range < IMMOBILITY_SVM_RANGE_THRESHOLD);
    
    // Debug: Print criteria evaluation
    Serial.println("  Criteria Check:");
    Serial.print("    Low Accel Variance: "); Serial.println(low_accel_variance ? "YES" : "NO");
    Serial.print("    Low Accel StdDev: "); Serial.println(low_accel_stddev ? "YES" : "NO");
    Serial.print("    Low Gyro Variance: "); Serial.println(low_gyro_variance ? "YES" : "NO");
    Serial.print("    Low SVM Range: "); Serial.println(low_svm_range ? "YES" : "NO");
    
    // Require at least 3 of 4 criteria to confirm immobility
    int immobility_criteria = 0;
    if (low_accel_variance) immobility_criteria++;
    if (low_accel_stddev) immobility_criteria++;
    if (low_gyro_variance) immobility_criteria++;
    if (low_svm_range) immobility_criteria++;
    
    Serial.print("  Criteria Met: "); Serial.print(immobility_criteria); Serial.println("/4");
    
    latest_event.is_immobile = (immobility_criteria >= 3);
    
    // Track immobility duration
    if (latest_event.is_immobile) {
      if (immobility_start_time == 0) {
        immobility_start_time = current_time;
      }
      latest_event.immobile_duration = current_time - immobility_start_time;
    } else {
      immobility_start_time = 0;
      latest_event.immobile_duration = 0;
    }
  }
  
  /**
   * Get current fall detection state
   */
  FallState getState() const {
    return current_state;
  }
  
  /**
   * Get latest fall event data
   */
  FallEvent getLatestEvent() const {
    return latest_event;
  }
  
  /**
   * Check if detector is calibrated
   */
  bool isCalibrated() const {
    return is_calibrated;
  }
  
  /**
   * Reset fall detector state
   */
  void reset() {
    current_state = NORMAL;
    impact_counter = 0;
    warning_counter = 0;
    gyro_sustained_counter = 0;
    latest_event.confirmed = false;
    latest_event.is_immobile = false;
    latest_event.immobile_duration = 0;
    immobility_buffer.sample_count = 0;
    immobility_buffer.sample_index = 0;
    immobility_start_time = 0;
    warning_start_time = 0;
    detected_freefall = false;
    detected_impact = false;
    detected_rotation = false;
    freefall_time = 0;
    impact_time = 0;
    vertical_motion_counter = 0;
  }
  
  /**
   * Print current detection parameters
   */
  void printConfiguration() {
    Serial.println("\n========================================");
    Serial.println("Fall Detector Configuration:");
    Serial.println("========================================");
    Serial.print("Jerk Threshold (High): "); Serial.print(JERK_THRESHOLD_HIGH); Serial.println(" m/s³");
    Serial.print("SVM Threshold (High):  "); Serial.print(SVM_THRESHOLD_HIGH); Serial.println(" g");
    Serial.print("SVM Threshold (Low):   "); Serial.print(SVM_THRESHOLD_LOW); Serial.println(" g");
    Serial.print("Gyro Threshold:        "); Serial.print(GYRO_THRESHOLD_COMBINED); Serial.println(" °/s");
    Serial.print("Pitch Threshold:       "); Serial.print(PITCH_THRESHOLD); Serial.println("°");
    Serial.print("Roll Threshold:        "); Serial.print(ROLL_THRESHOLD); Serial.println("°");
    Serial.println("--- Post-Fall Immobility Detection ---");
    Serial.print("Accel Variance Threshold: "); Serial.print(IMMOBILITY_ACCEL_VARIANCE_THRESHOLD); Serial.println(" (m/s²)²");
    Serial.print("Accel StdDev Threshold:   "); Serial.print(IMMOBILITY_ACCEL_STDDEV_THRESHOLD); Serial.println(" m/s²");
    Serial.print("Gyro Variance Threshold:  "); Serial.print(IMMOBILITY_GYRO_VARIANCE_THRESHOLD); Serial.println(" (°/s)²");
    Serial.print("SVM Range Threshold:      "); Serial.print(IMMOBILITY_SVM_RANGE_THRESHOLD); Serial.println(" g");
    Serial.print("Immobility Check Window:  "); Serial.print(IMMOBILITY_CHECK_WINDOW); Serial.println(" ms");
    Serial.println("========================================\n");
  }
  
  /**
   * Set sensitivity profile
   * @param profile 0=Conservative, 1=Balanced, 2=Sensitive
   */
  void setSensitivityProfile(uint8_t profile) {
    switch(profile) {
      case 0: // Conservative - fewer false positives (torso-optimized)
        JERK_THRESHOLD_HIGH = 450000.0f;
        JERK_THRESHOLD_MEDIUM = 250000.0f;
        SVM_THRESHOLD_HIGH = 2.0f;
        SVM_THRESHOLD_LOW = 0.6f;
        GYRO_THRESHOLD_COMBINED = 200.0f;
        GYRO_THRESHOLD_SUSTAINED = 140.0f;
        PITCH_THRESHOLD = 45.0f;
        ROLL_THRESHOLD = 38.0f;
        IMPACT_COUNT_THRESHOLD = 3;
        GYRO_SUSTAINED_COUNT = 4;
        Serial.println("Sensitivity Profile: CONSERVATIVE (Torso-Optimized)");
        break;
        
      case 1: // Balanced - default (torso-optimized)
        JERK_THRESHOLD_HIGH = 350000.0f;
        JERK_THRESHOLD_MEDIUM = 200000.0f;
        SVM_THRESHOLD_HIGH = 1.8f;
        SVM_THRESHOLD_LOW = 0.65f;
        GYRO_THRESHOLD_COMBINED = 180.0f;
        GYRO_THRESHOLD_SUSTAINED = 120.0f;
        PITCH_THRESHOLD = 40.0f;
        ROLL_THRESHOLD = 35.0f;
        IMPACT_COUNT_THRESHOLD = 2;
        GYRO_SUSTAINED_COUNT = 3;
        Serial.println("Sensitivity Profile: BALANCED (Torso-Optimized)");
        break;
        
      case 2: // Sensitive - maximum detection (torso-optimized)
        JERK_THRESHOLD_HIGH = 280000.0f;
        JERK_THRESHOLD_MEDIUM = 150000.0f;
        SVM_THRESHOLD_HIGH = 1.6f;
        SVM_THRESHOLD_LOW = 0.7f;
        GYRO_THRESHOLD_COMBINED = 160.0f;
        GYRO_THRESHOLD_SUSTAINED = 100.0f;
        PITCH_THRESHOLD = 35.0f;
        ROLL_THRESHOLD = 30.0f;
        IMPACT_COUNT_THRESHOLD = 1;
        GYRO_SUSTAINED_COUNT = 2;
        Serial.println("Sensitivity Profile: SENSITIVE (Torso-Optimized)");
        break;
    }
    printConfiguration();
  }
};

// ============================================================================
// MAX4466 MICROPHONE CLASS
// ============================================================================

/**
 * MAX4466 - Environmental Noise Monitoring
 * 
 * This class provides sound level measurement in decibels (dB)
 * for monitoring dangerous noise levels that could cause hearing damage.
 */
class MAX4466 {
private:
  uint8_t mic_pin;
  float db_offset;
  const uint16_t ADC_MAX_VALUE = 4095;
  const float VREF = 3.3;
  
public:
  // Noise level thresholds (dB)
  float DB_THRESHOLD_WARNING = 85.0;   // Risk with prolonged exposure
  float DB_THRESHOLD_DANGER = 100.0;   // Risk of immediate damage
  
  /**
   * Constructor
   * @param pin ADC pin connected to MAX4466 output
   * @param offset Calibration offset for dB readings
   */
  MAX4466(uint8_t pin, float offset = 16.0) {
    mic_pin = pin;
    db_offset = offset;
  }
  
  /**
   * Initialize the microphone
   */
  void begin() {
    analogReadResolution(12);  // 12-bit ADC (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range up to ~3.3V
    Serial.println("MAX4466 microphone initialized");
  }
  
  /**
   * Calculate dB from peak-to-peak amplitude
   * @param peakToPeak Peak-to-peak ADC value
   * @return Sound pressure level in dB
   */
  float calculateDB(float peakToPeak) {
    // Convert to voltage
    float voltage = (peakToPeak / ADC_MAX_VALUE) * VREF;
    
    // Calculate dB (SPL - Sound Pressure Level)
    // Using logarithmic scale: dB = 20 * log10(V/Vref) + offset
    float db = 20.0 * log10(voltage / 0.001) + db_offset;
    
    // Ensure reasonable range
    if (db < 0) db = 0;
    if (db > 120) db = 120;
    
    return db;
  }
  
  /**
   * Read current sound level
   * @param sampleWindow Sample window width in milliseconds
   * @return Sound level in dB
   */
  float readSoundLevel(uint32_t sampleWindow = 50) {
    unsigned long startMillis = millis();
    unsigned int signalMax = 0;
    unsigned int signalMin = ADC_MAX_VALUE;
    unsigned int sample;

    // Collect data for sample window
    while (millis() - startMillis < sampleWindow) {
      sample = analogRead(mic_pin);
      
      if (sample > signalMax) {
        signalMax = sample;
      }
      if (sample < signalMin) {
        signalMin = sample;
      }
    }
    
    unsigned int peakToPeak = signalMax - signalMin;
    return calculateDB(peakToPeak);
  }
  
  /**
   * Check noise level and return warning status
   * @param db Current sound level in dB
   * @return 0=Safe, 1=Warning, 2=Danger
   */
  uint8_t checkNoiseLevel(float db) {
    if (db >= DB_THRESHOLD_DANGER) {
      return 2;  // Danger
    } else if (db >= DB_THRESHOLD_WARNING) {
      return 1;  // Warning
    } else {
      return 0;  // Safe
    }
  }
  
  /**
   * Print noise level status
   * @param db Current sound level in dB
   */
  void printStatus(float db) {
    Serial.print("Sound Level: ");
    Serial.print(db, 1);
    Serial.print(" dB - ");
    
    uint8_t status = checkNoiseLevel(db);
    switch(status) {
      case 2:
        Serial.println("⚠️  DANGER! Extremely high noise! Protect ears immediately!");
        break;
      case 1:
        Serial.println("⚠️  WARNING! May damage hearing with prolonged exposure");
        break;
      case 0:
        Serial.println("✓ Safe noise level");
        break;
    }
  }
};

// ============================================================================
// AD8232 ECG MONITOR CLASS
// ============================================================================

/**
 * AD8232 - ECG/Heart Rate Monitoring
 * 
 * This class provides ECG signal processing and heart rate detection
 * for monitoring cardiac activity and detecting abnormalities.
 */
class AD8232 {
private:
  uint8_t ecg_pin;
  uint8_t lo_plus_pin;
  uint8_t lo_minus_pin;
  
  // ECG data buffers
  static const int BUFFER_SIZE = 200;  // 2 seconds at 100Hz
  int ecgDataBuffer[BUFFER_SIZE];
  int dataIndex;
  
  // Compressed ECG buffer for LoRaWAN transmission
  static const int COMPRESSED_SIZE = 50;  // Compressed samples
  uint8_t compressedECG[COMPRESSED_SIZE];
  int compressedIndex;
  
  // Downsampling (100Hz -> 25Hz, keep every 4th sample)
  int downsampleCounter;
  int lastCompressedValue;
  
  // PQRST wave detection buffer
  struct PQRSTWave {
    uint16_t timestamp;     // Relative timestamp (ms)
    int16_t p_amp;          // P wave amplitude
    int16_t q_amp;          // Q wave amplitude  
    int16_t r_amp;          // R wave amplitude
    int16_t s_amp;          // S wave amplitude
    int16_t t_amp;          // T wave amplitude
    uint8_t qrs_width;      // QRS duration (ms)
    uint8_t qt_interval;    // QT interval (ms)
  } lastPQRST;
  
  bool pqrstValid;
  
  // Heart rate detection
  unsigned long lastBeatTime;
  unsigned long beatInterval;
  int baselineValue;
  int peakValue;
  
  // ECG Features
  struct ECGFeatures {
    int rPeakAmplitude;      // R-wave amplitude
    int qrsWidth;            // QRS complex width (ms)
    int rrInterval;          // RR interval (ms)
    bool validBeat;
  } lastFeatures;
  
public:
  // Heart rate thresholds
  int BPM_MIN_NORMAL = 50;      // Minimum normal heart rate
  int BPM_MAX_NORMAL = 120;     // Maximum normal heart rate
  int BPM_MIN_VALID = 40;       // Minimum valid detection
  int BPM_MAX_VALID = 200;      // Maximum valid detection
  float THRESHOLD_PERCENT = 60.0; // R-wave detection threshold
  
  int currentBPM;
  bool leadsOff;
  
  /**
   * Constructor
   * @param ecg ADC pin for ECG signal
   * @param lo_plus Lead-off detection pin (LO+)
   * @param lo_minus Lead-off detection pin (LO-)
   */
  AD8232(uint8_t ecg, uint8_t lo_plus, uint8_t lo_minus) {
    ecg_pin = ecg;
    lo_plus_pin = lo_plus;
    lo_minus_pin = lo_minus;
    
    dataIndex = 0;
    lastBeatTime = 0;
    beatInterval = 0;
    baselineValue = 2048;  // 12-bit ADC midpoint
    peakValue = 0;
    currentBPM = 0;
    leadsOff = true;
    
    lastFeatures.validBeat = false;
    lastFeatures.rPeakAmplitude = 0;
    lastFeatures.qrsWidth = 0;
    lastFeatures.rrInterval = 0;
    
    // Initialize buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
      ecgDataBuffer[i] = 2048;
    }
    
    // Initialize compression buffers
    compressedIndex = 0;
    downsampleCounter = 0;
    lastCompressedValue = 2048;
    pqrstValid = false;
    
    for (int i = 0; i < COMPRESSED_SIZE; i++) {
      compressedECG[i] = 0;
    }
    
    memset(&lastPQRST, 0, sizeof(PQRSTWave));
  }
  
  /**
   * Initialize the ECG monitor
   */
  void begin() {
    pinMode(lo_plus_pin, INPUT);
    pinMode(lo_minus_pin, INPUT);
    analogReadResolution(12);  // 12-bit ADC (0-4095)
    analogSetAttenuation(ADC_11db);
    
    Serial.println("AD8232 ECG monitor initialized");
    Serial.println("Sample rate: 100 Hz");
  }
  
  /**
   * Check if ECG leads are properly connected
   * @return true if leads are off, false if connected
   */
  bool checkLeadsOff() {
    bool loPlus = digitalRead(lo_plus_pin);
    bool loMinus = digitalRead(lo_minus_pin);
    leadsOff = (loPlus == HIGH || loMinus == HIGH);
    
    if (leadsOff) {
      currentBPM = 0;
      lastBeatTime = 0;
    }
    
    return leadsOff;
  }
  
  /**
   * Read and process ECG signal
   * Detects R-peaks and calculates heart rate
   * @return Current ECG ADC value
   */
  int readECG() {
    if (checkLeadsOff()) {
      return 0;
    }
    
    int ecgValue = analogRead(ecg_pin);
    
    // Update data buffer
    ecgDataBuffer[dataIndex] = ecgValue;
    dataIndex = (dataIndex + 1) % BUFFER_SIZE;
    
    // Calculate dynamic baseline and threshold
    long sum = 0;
    int maxVal = 0;
    int minVal = 4095;
    
    for (int i = 0; i < BUFFER_SIZE; i++) {
      sum += ecgDataBuffer[i];
      if (ecgDataBuffer[i] > maxVal) maxVal = ecgDataBuffer[i];
      if (ecgDataBuffer[i] < minVal) minVal = ecgDataBuffer[i];
    }
    
    baselineValue = sum / BUFFER_SIZE;
    int amplitude = maxVal - minVal;
    int threshold = baselineValue + (amplitude * THRESHOLD_PERCENT / 100);
    
    // R-wave detection (rising edge over threshold)
    unsigned long currentTime = millis();
    if (ecgValue > threshold && (currentTime - lastBeatTime) > 300) {
      
      // Calculate RR interval
      if (lastBeatTime > 0) {
        beatInterval = currentTime - lastBeatTime;
        int bpm = 60000 / beatInterval;
        
        // Valid range check
        if (bpm >= BPM_MIN_VALID && bpm <= BPM_MAX_VALID) {
          currentBPM = bpm;
          lastFeatures.validBeat = true;
          lastFeatures.rrInterval = beatInterval;
          lastFeatures.rPeakAmplitude = ecgValue - baselineValue;
          lastFeatures.qrsWidth = 80;  // Typical 80-120ms
        }
      }
      
      lastBeatTime = currentTime;
      peakValue = ecgValue;
    }
    
    // Reset BPM if no beat for 3 seconds
    if (currentTime - lastBeatTime > 3000) {
      currentBPM = 0;
    }
    
    // === ECG Data Compression (Downsampling 100Hz -> 25Hz) ===
    downsampleCounter++;
    if (downsampleCounter >= 4) {  // Keep every 4th sample
      downsampleCounter = 0;
      
      // Differential encoding: store difference from previous value
      int diff = ecgValue - lastCompressedValue;
      
      // Compress 12-bit to 8-bit with clipping
      // Scale difference to fit in -128 to +127 range
      diff = constrain(diff / 4, -128, 127);
      
      // Store compressed sample
      compressedECG[compressedIndex] = (uint8_t)(diff + 128);  // Offset to 0-255
      compressedIndex = (compressedIndex + 1) % COMPRESSED_SIZE;
      
      lastCompressedValue = ecgValue;
    }
    
    return ecgValue;
  }
  
  /**
   * Get current heart rate in BPM
   * @return BPM (0 if no valid reading)
   */
  int getBPM() {
    return currentBPM;
  }
  
  /**
   * Get last detected ECG features
   */
  ECGFeatures getFeatures() {
    return lastFeatures;
  }
  
  /**
   * Check if heart rate is abnormal
   * @return 0=Normal, 1=Bradycardia (slow), 2=Tachycardia (fast), 3=No signal
   */
  uint8_t checkHeartRate() {
    if (leadsOff) return 3;
    if (currentBPM == 0) return 3;
    if (currentBPM < BPM_MIN_NORMAL) return 1;  // Too slow
    if (currentBPM > BPM_MAX_NORMAL) return 2;  // Too fast
    return 0;  // Normal
  }
  
  /**
   * Print ECG status and heart rate
   */
  void printStatus() {
    if (leadsOff) {
      Serial.println("ECG: Leads Off - Check connections");
      return;
    }
    
    Serial.print("Heart Rate: ");
    if (currentBPM > 0) {
      Serial.print(currentBPM);
      Serial.print(" BPM");
      
      uint8_t status = checkHeartRate();
      switch(status) {
        case 0:
          Serial.println(" - Normal");
          break;
        case 1:
          Serial.println(" - ⚠️  BRADYCARDIA (Too Slow!)");
          break;
        case 2:
          Serial.println(" - ⚠️  TACHYCARDIA (Too Fast!)");
          break;
      }
      
      if (lastFeatures.validBeat) {
        Serial.print("  RR Interval: ");
        Serial.print(lastFeatures.rrInterval);
        Serial.print(" ms  |  R-Peak: ");
        Serial.print(lastFeatures.rPeakAmplitude);
        Serial.println(" ADC units");
      }
    } else {
      Serial.println("-- BPM (Waiting for signal...)");
    }
  }
  
  /**
   * Print detailed heart beat information
   */
  void printBeatDetails() {
    if (!lastFeatures.validBeat || currentBPM == 0) return;
    
    Serial.println("\n=== Heart Beat Detected ===");
    Serial.print("BPM: "); Serial.println(currentBPM);
    Serial.print("RR Interval: "); Serial.print(lastFeatures.rrInterval); Serial.println(" ms");
    Serial.print("R Peak Amplitude: "); Serial.print(lastFeatures.rPeakAmplitude); Serial.println(" ADC units");
    Serial.print("QRS Width: ~"); Serial.print(lastFeatures.qrsWidth); Serial.println(" ms");
    Serial.print("Baseline: "); Serial.println(baselineValue);
    Serial.println("==========================\n");
  }
  
  /**
   * Extract PQRST wave features from ECG buffer
   * Simplified detection - finds key points relative to R peak
   */
  void extractPQRSTFeatures() {
    if (!lastFeatures.validBeat) {
      pqrstValid = false;
      return;
    }
    
    // Find R peak position in buffer (most recent beat)
    int rPeakPos = (dataIndex - 1 + BUFFER_SIZE) % BUFFER_SIZE;
    int rPeakValue = ecgDataBuffer[rPeakPos];
    
    // Search for Q wave (before R, local minimum)
    int qPos = rPeakPos;
    int qValue = rPeakValue;
    for (int i = 1; i <= 10 && i < BUFFER_SIZE; i++) {
      int pos = (rPeakPos - i + BUFFER_SIZE) % BUFFER_SIZE;
      if (ecgDataBuffer[pos] < qValue) {
        qValue = ecgDataBuffer[pos];
        qPos = pos;
      }
    }
    
    // Search for S wave (after R, local minimum)
    int sPos = rPeakPos;
    int sValue = rPeakValue;
    for (int i = 1; i <= 10 && i < BUFFER_SIZE; i++) {
      int pos = (rPeakPos + i) % BUFFER_SIZE;
      if (ecgDataBuffer[pos] < sValue) {
        sValue = ecgDataBuffer[pos];
        sPos = pos;
      }
    }
    
    // Search for P wave (before Q, small peak)
    int pPos = qPos;
    int pValue = baselineValue;
    for (int i = 5; i <= 25 && i < BUFFER_SIZE; i++) {
      int pos = (qPos - i + BUFFER_SIZE) % BUFFER_SIZE;
      if (ecgDataBuffer[pos] > pValue && ecgDataBuffer[pos] < rPeakValue) {
        pValue = ecgDataBuffer[pos];
        pPos = pos;
      }
    }
    
    // Search for T wave (after S, broader peak)
    int tPos = sPos;
    int tValue = baselineValue;
    for (int i = 10; i <= 40 && i < BUFFER_SIZE; i++) {
      int pos = (sPos + i) % BUFFER_SIZE;
      if (ecgDataBuffer[pos] > tValue && ecgDataBuffer[pos] < rPeakValue) {
        tValue = ecgDataBuffer[pos];
        tPos = pos;
      }
    }
    
    // Calculate QRS width (Q to S duration, samples * 10ms)
    int qrsWidth = abs(sPos - qPos) * 10;  // 100Hz = 10ms per sample
    
    // Calculate QT interval (Q to T duration)
    int qtInterval = abs(tPos - qPos) * 10;
    
    // Store PQRST features (relative to baseline)
    lastPQRST.timestamp = millis() & 0xFFFF;  // 16-bit timestamp
    lastPQRST.p_amp = pValue - baselineValue;
    lastPQRST.q_amp = qValue - baselineValue;
    lastPQRST.r_amp = rPeakValue - baselineValue;
    lastPQRST.s_amp = sValue - baselineValue;
    lastPQRST.t_amp = tValue - baselineValue;
    lastPQRST.qrs_width = constrain(qrsWidth, 0, 255);
    lastPQRST.qt_interval = constrain(qtInterval, 0, 255);
    
    pqrstValid = true;
  }
  
  /**
   * Get compressed ECG data for LoRaWAN transmission
   * Returns number of bytes written to output buffer
   * 
   * @param output Output buffer (must be at least COMPRESSED_SIZE bytes)
   * @param maxSize Maximum size of output buffer
   * @return Number of bytes written
   */
  int getCompressedECG(uint8_t* output, int maxSize) {
    int size = min(COMPRESSED_SIZE, maxSize);
    
    // Copy compressed ECG data
    for (int i = 0; i < size; i++) {
      int idx = (compressedIndex + i) % COMPRESSED_SIZE;
      output[i] = compressedECG[idx];
    }
    
    return size;
  }
  
  /**
   * Get PQRST wave features packed into bytes
   * Returns 14 bytes: timestamp(2) + amplitudes(10) + intervals(2)
   * 
   * @param output Output buffer (must be at least 14 bytes)
   * @return Number of bytes written (14 if valid, 0 if no valid PQRST)
   */
  int getPQRSTData(uint8_t* output) {
    if (!pqrstValid) return 0;
    
    int idx = 0;
    
    // Timestamp (2 bytes)
    output[idx++] = (lastPQRST.timestamp >> 8) & 0xFF;
    output[idx++] = lastPQRST.timestamp & 0xFF;
    
    // P amplitude (2 bytes, signed)
    output[idx++] = (lastPQRST.p_amp >> 8) & 0xFF;
    output[idx++] = lastPQRST.p_amp & 0xFF;
    
    // Q amplitude (2 bytes, signed)
    output[idx++] = (lastPQRST.q_amp >> 8) & 0xFF;
    output[idx++] = lastPQRST.q_amp & 0xFF;
    
    // R amplitude (2 bytes, signed)
    output[idx++] = (lastPQRST.r_amp >> 8) & 0xFF;
    output[idx++] = lastPQRST.r_amp & 0xFF;
    
    // S amplitude (2 bytes, signed)
    output[idx++] = (lastPQRST.s_amp >> 8) & 0xFF;
    output[idx++] = lastPQRST.s_amp & 0xFF;
    
    // T amplitude (2 bytes, signed)
    output[idx++] = (lastPQRST.t_amp >> 8) & 0xFF;
    output[idx++] = lastPQRST.t_amp & 0xFF;
    
    // QRS width (1 byte)
    output[idx++] = lastPQRST.qrs_width;
    
    // QT interval (1 byte)
    output[idx++] = lastPQRST.qt_interval;
    
    return 14;
  }
  
  /**
   * Get breathing rate estimated from ECG
   * Based on respiratory sinus arrhythmia (RSA)
   * @return Breathing rate in breaths per minute
   */
  int getBreathingRate() {
    // Simplified: Use RR interval variation
    // Typical breathing: 12-20 breaths/min
    // This is a placeholder - proper BR detection needs more complex analysis
    if (currentBPM == 0) return 0;
    
    // Estimate: BR is typically 1/4 to 1/5 of heart rate
    int estimatedBR = currentBPM / 4;
    return constrain(estimatedBR, 10, 30);  // Reasonable range
  }
};

// ============================================================================
// MLX90614 INFRARED TEMPERATURE SENSOR CLASS
// ============================================================================

/**
 * MLX90614 - Non-Contact IR Temperature Monitoring
 * 
 * This class provides body temperature measurement using infrared sensor
 * with advanced filtering for accurate readings.
 */
class MLX90614Sensor {
private:
  byte address;
  
  // Registers
  const byte REG_AMBIENT_TEMP = 0x06;
  const byte REG_OBJECT_TEMP = 0x07;
  
  // Moving average filter
  static const int FILTER_SIZE = 10;
  float tempHistory[FILTER_SIZE];
  int historyIndex;
  bool bufferFilled;
  
  /**
   * Read raw temperature from register
   */
  float readRawTemp(byte reg) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(address, (byte)3);
    
    if (Wire.available() >= 3) {
      uint16_t tempData = Wire.read();        // Low byte
      tempData |= Wire.read() << 8;           // High byte
      Wire.read();                            // PEC byte
      
      // Convert to Celsius: (rawValue * 0.02) - 273.15
      float tempCelsius = tempData * 0.02 - 273.15;
      return tempCelsius;
    }
    
    return NAN;
  }
  
  /**
   * Read with multi-sample averaging and outlier filtering
   */
  float readFilteredTemp(byte reg, int sampleCount = 20) {
    float samples[sampleCount];
    int validCount = 0;
    
    // Collect samples
    for (int i = 0; i < sampleCount; i++) {
      float temp = readRawTemp(reg);
      if (!isnan(temp)) {
        samples[validCount++] = temp;
      }
      delay(20);
    }
    
    if (validCount == 0) return NAN;
    
    // Sort samples
    for (int i = 0; i < validCount - 1; i++) {
      for (int j = i + 1; j < validCount; j++) {
        if (samples[i] > samples[j]) {
          float temp = samples[i];
          samples[i] = samples[j];
          samples[j] = temp;
        }
      }
    }
    
    // Remove outliers (top and bottom 20%)
    int removeCount = validCount / 5;
    int startIdx = removeCount;
    int endIdx = validCount - removeCount;
    
    // Calculate average
    float sum = 0;
    for (int i = startIdx; i < endIdx; i++) {
      sum += samples[i];
    }
    
    return sum / (endIdx - startIdx);
  }
  
  /**
   * Apply moving average filter
   */
  float applyMovingAverage(float value) {
    tempHistory[historyIndex] = value;
    historyIndex = (historyIndex + 1) % FILTER_SIZE;
    
    if (historyIndex == 0) bufferFilled = true;
    
    // Calculate average
    float sum = 0;
    int count = bufferFilled ? FILTER_SIZE : historyIndex;
    
    for (int i = 0; i < count; i++) {
      sum += tempHistory[i];
    }
    
    return (count > 0) ? (sum / count) : value;
  }
  
public:
  // Temperature thresholds (Celsius)
  float TEMP_TOO_LOW = 34.0;
  float TEMP_BELOW_NORMAL = 35.5;
  float TEMP_NORMAL_LOW = 36.5;
  float TEMP_NORMAL_HIGH = 37.0;
  float TEMP_SLIGHTLY_HIGH = 37.5;
  float TEMP_LOW_FEVER = 38.0;
  float TEMP_MODERATE_FEVER = 39.0;
  float TEMP_HIGH_FEVER = 40.0;
  float TEMP_TOO_HIGH = 42.0;
  
  float currentTemp;
  float ambientTemp;
  
  /**
   * Constructor
   */
  MLX90614Sensor(byte addr = 0x5A) {
    address = addr;
    historyIndex = 0;
    bufferFilled = false;
    currentTemp = 0;
    ambientTemp = 0;
    
    for (int i = 0; i < FILTER_SIZE; i++) {
      tempHistory[i] = 0;
    }
  }
  
  /**
   * Initialize sensor (uses existing I2C)
   */
  void begin() {
    Serial.println("MLX90614 IR temperature sensor initialized");
  }
  
  /**
   * Read ambient temperature
   */
  float readAmbient() {
    ambientTemp = readRawTemp(REG_AMBIENT_TEMP);
    return ambientTemp;
  }
  
  /**
   * Read body temperature with filtering
   */
  float readBodyTemp() {
    float rawTemp = readFilteredTemp(REG_OBJECT_TEMP, 20);
    
    if (isnan(rawTemp)) {
      return NAN;
    }
    
    currentTemp = applyMovingAverage(rawTemp);
    return currentTemp;
  }
  
  /**
   * Check temperature status
   * @return 0=Normal, 1=Below normal, 2=Slightly high, 3=Fever, 4=High fever, 5=Error
   */
  uint8_t checkTempStatus() {
    if (isnan(currentTemp)) return 5;
    if (currentTemp < TEMP_TOO_LOW || currentTemp > TEMP_TOO_HIGH) return 5;
    if (currentTemp < TEMP_BELOW_NORMAL) return 1;
    if (currentTemp >= TEMP_HIGH_FEVER) return 4;
    if (currentTemp >= TEMP_LOW_FEVER) return 3;
    if (currentTemp >= TEMP_SLIGHTLY_HIGH) return 2;
    return 0;  // Normal
  }
  
  /**
   * Print temperature status
   */
  void printStatus() {
    Serial.print("Body Temperature: ");
    
    if (isnan(currentTemp)) {
      Serial.println("Error - Cannot read sensor");
      return;
    }
    
    Serial.print(currentTemp, 2);
    Serial.print(" \u00b0C (Ambient: ");
    Serial.print(ambientTemp, 1);
    Serial.print(" \u00b0C) - ");
    
    uint8_t status = checkTempStatus();
    switch(status) {
      case 0:
        Serial.println("\u2713 Normal");
        break;
      case 1:
        Serial.println("\u26a0\ufe0f  Below Normal (Check sensor placement)");
        break;
      case 2:
        Serial.println("\u26a0\ufe0f  Slightly Elevated");
        break;
      case 3:
        Serial.println("FEVER Detected!");
        break;
      case 4:
        Serial.println("HIGH FEVER - Seek Medical Attention!");
        break;
      case 5:
        Serial.println("\u274c Sensor Error");
        break;
    }
  }
};

// ============================================================================
// LORA COMMUNICATION CLASS
// ============================================================================

/**
 * LoRaComm - Handle LoRa communication
 * 
 * Sends data packets via LoRa to Vision Master E213 receiver
 * Uses RadioLib for SX1262 LoRa module
 */

// Device ID (unique identifier for this device)
const char* DEVICE_ID = "ESP32-001";

// LoRa Configuration for Heltec WiFi LoRa 32 V3
// SX1262 Pins
const int LORA_NSS = 8;      // SPI NSS (Chip Select)
const int LORA_DIO1 = 14;    // DIO1
const int LORA_NRST = 12;    // Reset
const int LORA_BUSY = 13;    // Busy

// LoRa Parameters
const float LORA_FREQUENCY = 923.0;     // Frequency in MHz (923 for Hong Kong AS923)
const float LORA_BANDWIDTH = 125.0;     // Bandwidth in kHz
const uint8_t LORA_SPREADING_FACTOR = 9; // Spreading Factor (7-12)
const uint8_t LORA_CODING_RATE = 7;     // Coding Rate (5-8)
const int8_t LORA_OUTPUT_POWER = 22;    // TX Power in dBm (max 22)
const uint16_t LORA_PREAMBLE_LENGTH = 8; // Preamble length
const uint8_t LORA_SYNC_WORD = 0x12;    // Sync word (0x12 = private network)

// SX1262 LoRa module instance
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

class LoRaComm {
private:
  bool initialized;
  uint32_t lastTxTime;
  uint16_t frameCounter;
  int lastRssi;
  float lastSnr;
  
public:
  LoRaComm() {
    initialized = false;
    lastTxTime = 0;
    frameCounter = 0;
    lastRssi = 0;
    lastSnr = 0;
  }
  
  /**
   * Initialize LoRa module
   */
  bool begin() {
    Serial.println("\n🔧 Initializing LoRa SX1262...");
    
    // Initialize SX1262
    int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, 
                           LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER, 
                           LORA_PREAMBLE_LENGTH);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("✅ LoRa initialized successfully!");
      Serial.printf("   Frequency: %.1f MHz\n", LORA_FREQUENCY);
      Serial.printf("   Bandwidth: %.1f kHz\n", LORA_BANDWIDTH);
      Serial.printf("   Spreading Factor: %d\n", LORA_SPREADING_FACTOR);
      Serial.printf("   TX Power: %d dBm\n", LORA_OUTPUT_POWER);
      initialized = true;
      return true;
    } else {
      Serial.print("❌ LoRa initialization failed, code: ");
      Serial.println(state);
      return false;
    }
  }
  
  /**
   * Check if LoRa is initialized (compatibility method)
   */
  bool connect() {
    return initialized;
  }
  
  /**
   * Send data via LoRa
   * @param port Packet type (1=realtime, 2=ECG, 3=fall)
   * @param data Data buffer
   * @param len Data length
   * @param confirmed Not used (kept for compatibility)
   * @return true if sent successfully
   */
  bool sendUplink(uint8_t port, uint8_t* data, size_t len, bool confirmed = false) {
    if (!initialized) {
      Serial.println("❌ LoRa not initialized");
      return false;
    }
    
    // Create packet with header
    // Packet format: [Device ID (10 bytes)] [Frame Counter (2 bytes)] [Port (1 byte)] [Data (n bytes)]
    uint8_t packet[128];
    int packetLen = 0;
    
    // Add device ID (10 bytes, padded with zeros)
    memset(packet, 0, 10);
    strncpy((char*)packet, DEVICE_ID, 10);
    packetLen = 10;
    
    // Add frame counter (2 bytes, little-endian)
    packet[packetLen++] = frameCounter & 0xFF;
    packet[packetLen++] = (frameCounter >> 8) & 0xFF;
    
    // Add port/packet type (1 byte)
    packet[packetLen++] = port;
    
    // Add payload data
    memcpy(packet + packetLen, data, len);
    packetLen += len;
    
    // Send packet
    Serial.printf("📡 Sending LoRa packet (Type %d, %d bytes)...\n", port, packetLen);
    
    int state = radio.transmit(packet, packetLen);
    
    if (state == RADIOLIB_ERR_NONE) {
      // Get transmission info
      lastRssi = radio.getRSSI();
      lastSnr = radio.getSNR();
      
      Serial.println("✅ Packet sent successfully!");
      Serial.printf("   Frame: %d\n", frameCounter);
      Serial.printf("   Size: %d bytes\n", packetLen);
      
      frameCounter++;
      lastTxTime = millis();
      return true;
    } else {
      Serial.print("❌ LoRa transmission failed, code: ");
      Serial.println(state);
      return false;
    }
  }
  
  /**
   * Check if LoRa is initialized
   */
  bool isJoined() {
    return initialized;
  }
  
  /**
   * Get frame counter
   */
  uint16_t getFrameCounter() {
    return frameCounter;
  }
  
  /**
   * Get last RSSI
   */
  int getRSSI() {
    return lastRssi;
  }
  
  /**
   * Get last SNR
   */
  float getSNR() {
    return lastSnr;
  }
  
  /**
   * Check LoRa status (compatibility method)
   */
  void maintain() {
    // LoRa doesn't need connection maintenance
    // This method kept for compatibility
  }
};

// ============================================================================
// PAYLOAD BUILDER
// ============================================================================

/**
 * Build payloads for different data types
 * Same format as LoRaWAN version - binary packed data
 */
class PayloadBuilder {
public:
  /**
   * Build real-time monitoring payload (Packet Type 0x01)
   * Sent every 5-10 minutes
   * 
   * Format:
   * [0] Packet type: 0x01
   * [1] Heart rate (BPM): uint8
   * [2] Body temperature (-20 to 80°C mapped to 0-255): int8
   * [3] Ambient temperature (-20 to 80°C mapped to 0-255): int8
   * [4] Noise level (dB): uint8
   * [5] Fall state: uint8 (0=Normal, 1=Warning, 2=Fall, 3=Dangerous, 4=Recovery)
   * [6] Alert flags: uint8 (bit0=HR abnormal, bit1=Temp abnormal, bit2=Fall, bit3=Noise)
   * [7-8] RSSI: int16
   * [9] SNR: int8
   * Total: 10 bytes
   */
  static int buildRealtimePayload(uint8_t* buffer,
                                   int bpm,
                                   float bodyTemp,
                                   float ambientTemp,
                                   float noisedB,
                                   uint8_t fallState,
                                   bool hrAbnormal,
                                   bool tempAbnormal,
                                   bool fallAlert,
                                   bool noiseAlert) {
    int idx = 0;
    
    buffer[idx++] = 0x01;  // Packet type
    buffer[idx++] = constrain(bpm, 0, 255);
    buffer[idx++] = tempToInt8(bodyTemp);
    buffer[idx++] = tempToInt8(ambientTemp);
    buffer[idx++] = constrain((int)noisedB, 0, 255);
    buffer[idx++] = fallState;
    
    // Alert flags
    uint8_t flags = 0;
    if (hrAbnormal) flags |= 0x01;
    if (tempAbnormal) flags |= 0x02;
    if (fallAlert) flags |= 0x04;
    if (noiseAlert) flags |= 0x08;
    buffer[idx++] = flags;
    
    // Placeholder for RSSI/SNR (filled by radio)
    buffer[idx++] = 0;
    buffer[idx++] = 0;
    buffer[idx++] = 0;
    
    return idx;
  }
  
  /**
   * Build ECG data payload (Packet Type 0x02)
   * Sent on heartbeat or abnormal condition
   * 
   * Format:
   * [0] Packet type: 0x02
   * [1-50] Compressed ECG (25Hz, 2 seconds, differential encoding)
   * [51-64] PQRST features (timestamp + 5 amplitudes + 2 intervals)
   * Total: 65 bytes
   */
  static int buildECGPayload(uint8_t* buffer,
                             uint8_t* compressedECG,
                             int ecgLen,
                             uint8_t* pqrst,
                             int pqrstLen) {
    int idx = 0;
    
    buffer[idx++] = 0x02;  // Packet type
    
    // Copy compressed ECG
    memcpy(&buffer[idx], compressedECG, min(ecgLen, 50));
    idx += 50;
    
    // Copy PQRST features
    if (pqrstLen > 0) {
      memcpy(&buffer[idx], pqrst, min(pqrstLen, 14));
      idx += 14;
    } else {
      // Fill with zeros if no PQRST data
      memset(&buffer[idx], 0, 14);
      idx += 14;
    }
    
    return idx;
  }
  
  /**
   * Build fall event payload (Packet Type 0x03)
   * Sent only when fall is detected
   * 
   * Format:
   * [0] Packet type: 0x03
   * [1-4] Timestamp: uint32
   * [5-8] Jerk magnitude: float32
   * [9-12] SVM value: float32  
   * [13-16] Angular velocity: float32
   * [17-20] Pitch angle: float32
   * [21-24] Roll angle: float32
   * [25] Impact counter: uint8
   * [26] Warning counter: uint8
   * [27] Heart rate: uint8
   * [28] Body temperature: int8
   * [29-32] Accel X: float32
   * [33-36] Accel Y: float32
   * [37-40] Accel Z: float32
   * [41-44] Movement variance: float32
   * Total: 45 bytes
   */
  static int buildFallEventPayload(uint8_t* buffer,
                                    uint32_t timestamp,
                                    float jerk,
                                    float svm,
                                    float angularVel,
                                    float pitch,
                                    float roll,
                                    uint8_t impactCount,
                                    uint8_t warningCount,
                                    int bpm,
                                    float bodyTemp,
                                    float accelX,
                                    float accelY,
                                    float accelZ,
                                    float movementVar) {
    int idx = 0;
    
    buffer[idx++] = 0x03;  // Packet type
    
    // Timestamp
    memcpy(&buffer[idx], &timestamp, 4);
    idx += 4;
    
    // Fall metrics
    memcpy(&buffer[idx], &jerk, 4);
    idx += 4;
    memcpy(&buffer[idx], &svm, 4);
    idx += 4;
    memcpy(&buffer[idx], &angularVel, 4);
    idx += 4;
    memcpy(&buffer[idx], &pitch, 4);
    idx += 4;
    memcpy(&buffer[idx], &roll, 4);
    idx += 4;
    
    // Counters
    buffer[idx++] = impactCount;
    buffer[idx++] = warningCount;
    
    // Vital signs
    buffer[idx++] = constrain(bpm, 0, 255);
    buffer[idx++] = tempToInt8(bodyTemp);
    
    // Acceleration data
    memcpy(&buffer[idx], &accelX, 4);
    idx += 4;
    memcpy(&buffer[idx], &accelY, 4);
    idx += 4;
    memcpy(&buffer[idx], &accelZ, 4);
    idx += 4;
    
    // Movement variance
    memcpy(&buffer[idx], &movementVar, 4);
    idx += 4;
    
    return idx;
  }
  
private:
  /**
   * Map temperature (-20°C to 80°C) to int8 (0-255)
   */
  static int8_t tempToInt8(float temp) {
    // Map -20°C to 80°C -> 0 to 255 range
    int mapped = (int)((temp + 20.0) / 100.0 * 255.0);
    return constrain(mapped, 0, 255);
  }
};

// ============================================================================
// Configuration Constants
// ============================================================================

// I2C Pin Configuration for ESP32
const int PIN_SDA = 48;        // I2C Data line
const int PIN_SCL = 47;        // I2C Clock line
const int I2C_FREQUENCY = 100000;  // I2C clock speed (100kHz - standard mode)

// MAX4466 Microphone Configuration
const int PIN_MIC = 3;         // GPIO 3 (ADC1_CH2) for MAX4466
const int MIC_SAMPLE_WINDOW = 50;  // Sample window in ms

// AD8232 ECG Configuration
const int PIN_ECG = 1;         // GPIO 1 (ADC1_CH0) for ECG signal
const int PIN_LO_PLUS = 9;    // GPIO 9 for LO+ lead-off detection
const int PIN_LO_MINUS = 10;   // GPIO 10 for LO- lead-off detection
const int ECG_SAMPLE_INTERVAL = 10;  // 100 Hz sampling

// Timing Configuration
const int SERIAL_BAUD_RATE = 115200;  // Serial communication speed
const int READ_INTERVAL_MS = 500;     // Sensor reading interval

// ============================================================================
// Global Objects
// ============================================================================

MPU6050 mpu;              // MPU6050 sensor object
FallDetector fallDetector; // Fall detection algorithm instance
MAX4466 microphone(PIN_MIC); // MAX4466 microphone object
AD8232 ecgMonitor(PIN_ECG, PIN_LO_PLUS, PIN_LO_MINUS); // AD8232 ECG monitor
MLX90614Sensor tempSensor; // MLX90614 temperature sensor
LoRaComm loraComm;        // LoRa communication object

// ECG sampling timing
unsigned long lastECGSampleTime = 0;
// Temperature sampling timing
unsigned long lastTempSampleTime = 0;
const int TEMP_SAMPLE_INTERVAL = 5000;  // Read temperature every 5 seconds

// WiFi transmission timing
unsigned long lastRealtimeTxTime = 0;
unsigned long lastECGTxTime = 0;  // Track ECG transmission time globally
const unsigned long REALTIME_TX_INTERVAL = 60000;   // Send realtime data every 60s (1 minute)
const unsigned long ECG_TX_INTERVAL = 306000;       // Send ECG data every 306s (5.1 minutes)
bool fallEventTriggered = false;

// State change tracking for immediate notifications
FallDetector::FallState previousFallState = FallDetector::NORMAL;
bool stateChangeNotified = false;

// Maximum noise tracking
float maxNoisedB = 0.0;  // Track maximum noise level between transmissions
unsigned long maxNoiseTimestamp = 0;  // When max noise occurred

// Helper function to format time remaining
String formatTimeRemaining(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  seconds = seconds % 60;
  
  if (minutes > 0) {
    return String(minutes) + "m " + String(seconds) + "s";
  } else {
    return String(seconds) + "s";
  }
}

// ============================================================================
// I2C Scanner Function
// ============================================================================

/**
 * Scan I2C bus for connected devices
 * Useful for debugging connection issues
 */
void scanI2CBus() {
  Serial.println("Scanning I2C bus...");
  
  int devicesFound = 0;
  
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("  Device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      devicesFound++;
    }
  }
  
  if (devicesFound == 0) {
    Serial.println("  No I2C devices found!");
  } else {
    Serial.print("  Total devices found: ");
    Serial.println(devicesFound);
  }
  
  Serial.println();
}

// ============================================================================
// Arduino Setup Function
// ============================================================================

void setup() {
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  delay(100);
  
  // Print header
  Serial.println("\n========================================");
  Serial.println(" Comprehensive Health Monitoring");
  Serial.println("   Fall + ECG + Temp + Noise");
  Serial.println("========================================\n");
  
  // Initialize I2C with custom pins
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_FREQUENCY);
  delay(100);
  
  Serial.print("I2C initialized: SDA=GPIO");
  Serial.print(PIN_SDA);
  Serial.print(", SCL=GPIO");
  Serial.print(PIN_SCL);
  Serial.print(", Frequency=");
  Serial.print(I2C_FREQUENCY / 1000);
  Serial.println("kHz\n");
  
  // Scan I2C bus for debugging
  scanI2CBus();
  
  // Initialize MPU6050 sensor
  Serial.println("Initializing MPU6050...");
  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 initialization failed!");
    Serial.println("Please check your wiring and connections.");
    while (1) {
      delay(1000);  // Halt program
    }
  }
  
  Serial.println("\nSensor ready! Starting measurements...\n");
  delay(500);
  
  // ========================================
  // Temperature Sensor Initialization
  // ========================================
  
  Serial.println("Initializing MLX90614 IR temperature sensor...");
  tempSensor.begin();
  Serial.println("Temperature sensor ready!\n");
  
  Serial.println("Body Temperature Guidelines:");
  Serial.println("  35.5-37.0\u00b0C: Normal underarm range");
  Serial.println("  37.5-38.0\u00b0C: Slightly elevated");
  Serial.println("  > 38.0\u00b0C: Fever detected\n");
  
  // ========================================
  // ECG Monitor Initialization
  // ========================================
  
  Serial.println("Initializing AD8232 ECG monitor...");
  ecgMonitor.begin();
  Serial.println("ECG monitor ready!\n");
  
  Serial.println("Heart Rate Guidelines:");
  Serial.println("  50-120 BPM: Normal range");
  Serial.println("  < 50 BPM: Bradycardia (too slow)");
  Serial.println("  > 120 BPM: Tachycardia (too fast)\n");
  
  // ========================================
  // Microphone Initialization
  // ========================================
  
  Serial.println("Initializing MAX4466 microphone...");
  microphone.begin();
  Serial.println("Microphone ready!\n");
  
  Serial.println("Noise Level Guidelines:");
  Serial.println("  < 85 dB: Safe");
  Serial.println("  85-100 dB: Risk with prolonged exposure");
  Serial.println("  > 100 dB: Risk of immediate hearing damage\n");
  
  // ========================================
  // Fall Detector Initialization
  // ========================================
  
  // Set sensitivity profile (0=Conservative, 1=Balanced, 2=Sensitive)
  // Change this value to adjust detection sensitivity
  fallDetector.setSensitivityProfile(2); // Default: Balanced
  
  // Calibration phase - collect baseline posture data
  Serial.println("Starting calibration... Please keep device still in normal position.");
  delay(2000);
  
  float total_pitch = 0;
  float total_roll = 0;
  const int CALIBRATION_SAMPLES = 20;
  
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    MPU6050::SensorData data = mpu.readSensorData();
    
    // Calculate pitch and roll from accelerometer
    float pitch = atan2(data.accelY, data.accelZ) * 180.0f / PI;
    float roll = atan2(data.accelX, data.accelZ) * 180.0f / PI;
    
    total_pitch += pitch;
    total_roll += roll;
    
    delay(50);
  }
  
  float avg_pitch = total_pitch / CALIBRATION_SAMPLES;
  float avg_roll = total_roll / CALIBRATION_SAMPLES;
  
  fallDetector.calibrate(avg_pitch, avg_roll);
  
  Serial.println("========================================");
  Serial.println("  System Initialization Complete!");
  Serial.println("========================================");
  Serial.println("✓ MPU6050 Fall Detection: Active");
  Serial.println("✓ AD8232 ECG Monitoring: Active");
  Serial.println("✓ MLX90614 Temperature: Active");
  Serial.println("✓ MAX4466 Noise Monitoring: Active");
  Serial.println("========================================\n");
  
  // ========================================
  // LoRa Initialization
  // ========================================
  
  Serial.println("Initializing LoRa...");
  if (!loraComm.begin()) {
    Serial.println("ERROR: LoRa initialization failed!");
    Serial.println("System halted. Check wiring and restart.");
    while(1) delay(1000);  // Halt - LoRa is critical
  } else {
    Serial.println("✅ LoRa initialized successfully!");
    Serial.println("\n========================================");
    Serial.println("  LORA READY");
    Serial.println("========================================");
    Serial.println("Frequency: 923 MHz (Hong Kong AS923)");
    Serial.println("Spreading Factor: 9");
    Serial.println("Bandwidth: 125 kHz");
    Serial.println("========================================\n");
  }
  
  Serial.println("\n========================================");
  Serial.println("  ALL SYSTEMS READY");
  Serial.println("========================================");
  
  Serial.println("\n📡 TRANSMISSION SCHEDULE:");
  Serial.println("  ┌─────────────────────────────────────────┐");
  Serial.print("  │ Realtime Data:     Every ");
  Serial.print(REALTIME_TX_INTERVAL / 1000);
  Serial.println("s (1min)     │");
  Serial.print("  │ ECG Data:          Every ");
  Serial.print(ECG_TX_INTERVAL / 1000);
  Serial.println("s (5.1min) │");
  Serial.println("  │ Fall Events:       Immediate            │");
  Serial.println("  │                                         │");
  Serial.println("  │ Note: Transmission times are staggered  │");
  Serial.println("  │       to avoid network congestion       │");
  Serial.println("  └─────────────────────────────────────────┘");
  Serial.println();
  
  Serial.println("Starting monitoring loop...\n");
}

// ============================================================================
// Arduino Main Loop
// ============================================================================

void loop() {
  unsigned long currentTime = millis();
  
  // Display next transmission countdown (every 10 seconds)
  static unsigned long lastCountdownDisplay = 0;
  if (currentTime - lastCountdownDisplay >= 10000) {
    unsigned long realtimeRemaining = (lastRealtimeTxTime + REALTIME_TX_INTERVAL > currentTime) ? 
                                      (lastRealtimeTxTime + REALTIME_TX_INTERVAL - currentTime) : 0;
    unsigned long ecgRemaining = (lastECGTxTime + ECG_TX_INTERVAL > currentTime) ? 
                                 (lastECGTxTime + ECG_TX_INTERVAL - currentTime) : 0;
    
    Serial.println("\n⏰ TRANSMISSION COUNTDOWN:");
    Serial.print("  Realtime (1min):  ");
    if (realtimeRemaining > 0) {
      Serial.println(formatTimeRemaining(realtimeRemaining));
    } else {
      Serial.println("Ready to send!");
    }
    Serial.print("  ECG (5.1min):     ");
    if (ecgRemaining > 0) {
      Serial.println(formatTimeRemaining(ecgRemaining));
    } else {
      Serial.println("Ready to send!");
    }
    Serial.println();
    lastCountdownDisplay = currentTime;
  }
  
  // Read ECG data at 100 Hz
  if (currentTime - lastECGSampleTime >= ECG_SAMPLE_INTERVAL) {
    int ecgValue = ecgMonitor.readECG();
    
    // Extract PQRST features when new heartbeat detected
    if (ecgMonitor.currentBPM > 0 && ecgValue > 0) {
      static unsigned long lastPQRSTExtraction = 0;
      if (currentTime - lastPQRSTExtraction > 1000) {  // Extract every second
        ecgMonitor.extractPQRSTFeatures();
        lastPQRSTExtraction = currentTime;
      }
    }
    
    lastECGSampleTime = currentTime;
  }
  
  // Read temperature every 5 seconds
  if (currentTime - lastTempSampleTime >= TEMP_SAMPLE_INTERVAL) {
    tempSensor.readAmbient();
    tempSensor.readBodyTemp();
    lastTempSampleTime = currentTime;
  }
  
  // Read sensor data
  MPU6050::SensorData data = mpu.readSensorData();
  
  // Perform fall detection
  FallDetector::FallEvent fall_event = fallDetector.detectFall(data);
  
  // Read noise level
  float soundLevel = microphone.readSoundLevel(MIC_SAMPLE_WINDOW);
  
  // Track maximum noise level between transmissions
  if (soundLevel > maxNoisedB) {
    maxNoisedB = soundLevel;
    maxNoiseTimestamp = currentTime;
  }
  
  // Display sensor data
  mpu.printData(data);
  
  // Display body temperature monitoring
  Serial.println("--- Body Temperature Monitoring ---");
  tempSensor.printStatus();
  
  // Display ECG/Heart rate monitoring
  Serial.println("--- Heart Rate Monitoring ---");
  ecgMonitor.printStatus();
  
  // Display ECG compression info (every 5 seconds)
  static unsigned long lastCompressionInfo = 0;
  if (currentTime - lastCompressionInfo > 5000) {
    Serial.println("--- ECG Data Compression Status ---");
    
    // Show compressed ECG size
    uint8_t tempBuffer[50];
    int compressedSize = ecgMonitor.getCompressedECG(tempBuffer, 50);
    Serial.print("Compressed ECG: ");
    Serial.print(compressedSize);
    Serial.println(" bytes (25Hz, 8-bit differential)");
    
    // Show PQRST data size
    int pqrstSize = ecgMonitor.getPQRSTData(tempBuffer);
    if (pqrstSize > 0) {
      Serial.print("PQRST Features: ");
      Serial.print(pqrstSize);
      Serial.println(" bytes");
      Serial.println("  P/Q/R/S/T amplitudes + QRS width + QT interval");
    }
    
    // Show breathing rate
    int br = ecgMonitor.getBreathingRate();
    if (br > 0) {
      Serial.print("Estimated Breathing Rate: ");
      Serial.print(br);
      Serial.println(" breaths/min");
    }
    
    Serial.println();
    lastCompressionInfo = currentTime;
  }
  
  // Display noise monitoring
  Serial.println("--- Environmental Noise Monitoring ---");
  microphone.printStatus(soundLevel);
  if (maxNoisedB > 0) {
    Serial.print("Max Noise Since Last Tx: ");
    Serial.print(maxNoisedB, 1);
    Serial.print(" dB (at ");
    Serial.print(formatTimeRemaining(currentTime - maxNoiseTimestamp));
    Serial.println(" ago)");
  }
  
  // Display fall detection status
  Serial.println("--- Fall Detection Status ---");
  
  // Show current state
  Serial.print("State: ");
  switch(fall_event.state) {
    case FallDetector::NORMAL:
      Serial.println("NORMAL");
      break;
    case FallDetector::WARNING:
      Serial.print("WARNING (Impact Count: ");
      Serial.print("...)");
      Serial.println();
      break;
    case FallDetector::FALL_DETECTED:
      Serial.println("*** FALL DETECTED ***");
      break;
    case FallDetector::DANGEROUS:
      Serial.println("*** DANGEROUS - IMMOBILE/UNCONSCIOUS ***");
      break;
    case FallDetector::RECOVERY:
      Serial.println("RECOVERY");
      break;
  }
  
  // Show detection metrics
  Serial.print("Jerk: ");
  Serial.print(fall_event.jerk_magnitude, 0);
  Serial.print(" m/s³  |  SVM: ");
  Serial.print(fall_event.svm_value, 2);
  Serial.print(" g  |  Angular Vel: ");
  Serial.print(fall_event.angular_velocity, 1);
  Serial.println(" °/s");
  
  if (fallDetector.isCalibrated()) {
    Serial.print("Pitch: ");
    Serial.print(fall_event.pitch_angle, 1);
    Serial.print("°  |  Roll: ");
    Serial.print(fall_event.roll_angle, 1);
    Serial.println("°");
  }
  
  // Show post-fall movement monitoring
  if (fall_event.state == FallDetector::FALL_DETECTED || 
      fall_event.state == FallDetector::DANGEROUS ||
      fall_event.state == FallDetector::RECOVERY) {
    Serial.println("--- Post-Fall Movement Analysis ---");
    Serial.print("Movement Variance: ");
    Serial.print(fall_event.movement_variance, 4);
    Serial.print(" (m/s²)²  |  StdDev: ");
    Serial.print(fall_event.movement_stddev, 3);
    Serial.println(" m/s²");
    
    Serial.print("Immobile: ");
    Serial.print(fall_event.is_immobile ? "YES" : "NO");
    if (fall_event.is_immobile) {
      Serial.print("  |  Duration: ");
      Serial.print(fall_event.immobile_duration / 1000.0f, 1);
      Serial.print(" seconds");
    }
    Serial.println();
  }
  
  // Alert if fall confirmed
  if (fall_event.confirmed) {
    Serial.println("\n!!! FALL CONFIRMED !!!");
    Serial.println("!!! EMERGENCY ALERT TRIGGERED !!!");
    Serial.print("!!! Timestamp: ");
    Serial.print(fall_event.timestamp);
    Serial.println(" ms !!!");
    Serial.println("!!! Monitoring for movement... !!!");
    
    // Include vital signs in emergency alert
    int bpm = ecgMonitor.getBPM();
    if (bpm > 0) {
      Serial.print("!!! Heart Rate: ");
      Serial.print(bpm);
      Serial.println(" BPM !!!");
    }
    
    float bodyTemp = tempSensor.currentTemp;
    if (!isnan(bodyTemp)) {
      Serial.print("!!! Body Temperature: ");
      Serial.print(bodyTemp, 1);
      Serial.println(" \u00b0C !!!");
    }
    Serial.println();
    
    // Here you can add:
    // - Send emergency SMS/notification
    // - Activate buzzer/LED
    // - Log event to SD card
    // - Send data to cloud/server
  }
  
  // Critical alert if person is immobile/unconscious
  if (fall_event.state == FallDetector::DANGEROUS) {
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║  ⚠️  CRITICAL: NO MOVEMENT DETECTED  ⚠️  ║");
    Serial.println("║  POSSIBLE UNCONSCIOUSNESS/INJURY     ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.print("Immobile for: ");
    Serial.print(fall_event.immobile_duration / 1000.0f, 1);
    Serial.println(" seconds");
    Serial.print("Movement Variance: ");
    Serial.println(fall_event.movement_variance, 4);
    
    // Include all vital signs
    int bpm = ecgMonitor.getBPM();
    uint8_t hrStatus = ecgMonitor.checkHeartRate();
    Serial.print("Heart Rate: ");
    if (bpm > 0) {
      Serial.print(bpm);
      Serial.print(" BPM");
      if (hrStatus == 1) Serial.println(" - ABNORMALLY LOW!");
      else if (hrStatus == 2) Serial.println(" - ABNORMALLY HIGH!");
      else Serial.println();
    } else {
      Serial.println("NO SIGNAL");
    }
    
    float bodyTemp = tempSensor.currentTemp;
    Serial.print("Body Temperature: ");
    if (!isnan(bodyTemp)) {
      Serial.print(bodyTemp, 1);
      Serial.print(" \u00b0C");
      uint8_t tempStatus = tempSensor.checkTempStatus();
      if (tempStatus == 3 || tempStatus == 4) Serial.println(" - FEVER!");
      else Serial.println();
    } else {
      Serial.println("NO READING");
    }
    
    Serial.println("IMMEDIATE EMERGENCY RESPONSE REQUIRED!\n");
    
    // Here you can add:
    // - Send URGENT emergency notification
    // - Activate high-priority alarm
    // - Automatically call emergency services
    // - Send GPS location to emergency contacts
    // - Activate strobe lights for visibility
  }
  
  // Alert for abnormal heart rate
  uint8_t hrStatus = ecgMonitor.checkHeartRate();
  if (hrStatus == 1) {
    Serial.println("\n⚠️  HEART RATE ALERT: BRADYCARDIA (Too Slow!) ⚠️");
    Serial.print("Current BPM: ");
    Serial.println(ecgMonitor.getBPM());
  } else if (hrStatus == 2) {
    Serial.println("\n⚠️  HEART RATE ALERT: TACHYCARDIA (Too Fast!) ⚠️");
    Serial.print("Current BPM: ");
    Serial.println(ecgMonitor.getBPM());
  }
  
  // Alert for abnormal body temperature
  uint8_t tempStatus = tempSensor.checkTempStatus();
  if (tempStatus == 3) {
    Serial.println("\n🌡️  TEMPERATURE ALERT: FEVER DETECTED! 🌡️");
    Serial.print("Current Temperature: ");
    Serial.print(tempSensor.currentTemp, 1);
    Serial.println(" °C");
  } else if (tempStatus == 4) {
    Serial.println("\n🔥 CRITICAL TEMPERATURE ALERT: HIGH FEVER! 🔥");
    Serial.print("Current Temperature: ");
    Serial.print(tempSensor.currentTemp, 1);
    Serial.println(" °C");
    Serial.println("SEEK MEDICAL ATTENTION IMMEDIATELY!");
  }
  
  Serial.println();
  
  // ========================================
  // Check for critical state changes
  // ========================================
  // Send immediate realtime packet if state changed to DANGEROUS or returned to NORMAL
  if (fall_event.state != previousFallState) {
    if (fall_event.state == FallDetector::FALL_DETECTED) {
      Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
      Serial.println("║  ⚠️  STATE CHANGE: FALL DETECTED - SENDING IMMEDIATE ALERT ║");
      Serial.println("╚═══════════════════════════════════════════════════════════╝");
      stateChangeNotified = true;
    } else if (fall_event.state == FallDetector::DANGEROUS) {
      Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
      Serial.println("║  🚨 STATE CHANGE: UNCONSCIOUS - SENDING IMMEDIATE ALERT  ║");
      Serial.println("╚═══════════════════════════════════════════════════════════╝");
      stateChangeNotified = true;
    } else if ((previousFallState == FallDetector::FALL_DETECTED || previousFallState == FallDetector::DANGEROUS) && fall_event.state == FallDetector::NORMAL) {
      Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
      Serial.println("║  ✅ STATE CHANGE: RECOVERED - SENDING IMMEDIATE UPDATE   ║");
      Serial.println("╚═══════════════════════════════════════════════════════════╝");
      stateChangeNotified = true;
    }
    previousFallState = fall_event.state;
  }
  
  // Send immediate realtime packet on critical state change
  if (stateChangeNotified) {
    Serial.println("\n📡 Sending immediate realtime packet (State Change Alert)...");
    
    uint8_t payload[10];
    
    // Check alert conditions
    uint8_t hrStatus = ecgMonitor.checkHeartRate();
    uint8_t tempStatus = tempSensor.checkTempStatus();
    bool hrAbnormal = (hrStatus != 0);
    bool tempAbnormal = (tempStatus >= 3);
    bool fallAlert = (fall_event.state >= FallDetector::FALL_DETECTED);
    bool noiseAlert = (maxNoisedB >= 80.0);  // Noise alert threshold
    
    // Use current or max noise level
    float noiseToSend = (maxNoisedB > 0) ? maxNoisedB : soundLevel;
    
    int len = PayloadBuilder::buildRealtimePayload(
      payload,
      ecgMonitor.getBPM(),
      tempSensor.currentTemp,
      tempSensor.ambientTemp,
      noiseToSend,
      (uint8_t)fall_event.state,
      hrAbnormal,
      tempAbnormal,
      fallAlert,
      noiseAlert
    );
    
    bool success = loraComm.sendUplink(1, payload, len);
    
    if (success) {
      Serial.println("✅ Immediate packet sent successfully!");
      Serial.print("State: ");
      if (fall_event.state == FallDetector::DANGEROUS) {
        Serial.println("UNCONSCIOUS");
      } else {
        Serial.println("RECOVERED");
      }
      Serial.print("BPM: ");
      Serial.print(ecgMonitor.getBPM());
      Serial.print("  Temp: ");
      Serial.print(tempSensor.currentTemp, 1);
      Serial.print("°C  Noise: ");
      Serial.print(noiseToSend, 1);
      Serial.println("dB");
    } else {
      Serial.println("❌ Failed to send immediate packet");
    }
    
    stateChangeNotified = false;  // Reset flag
    Serial.println();
  }
  
  // ========================================
  // WiFi Data Transmission
  // ========================================
  
  // LoRa doesn't need connection maintenance
  // loraComm.maintain();
  
  // LoRa is always ready (no join procedure needed)
  // Proceed directly to sensor monitoring and transmission
    // Send fall event immediately (Packet Type 0x03)
    if (fall_event.confirmed && !fallEventTriggered) {
      Serial.println("\n╔══════════════════════════════════════════════════════════╗");
      Serial.println("║        📡 FALL EVENT TRANSMISSION (Type 0x03)          ║");
      Serial.println("╚══════════════════════════════════════════════════════════╝");
      
      uint8_t payload[50];
      int bpm = ecgMonitor.getBPM();
      float bodyTemp = tempSensor.currentTemp;
      
      int len = PayloadBuilder::buildFallEventPayload(
        payload,
        fall_event.timestamp,
        fall_event.jerk_magnitude,
        fall_event.svm_value,
        fall_event.angular_velocity,
        fall_event.pitch_angle,
        fall_event.roll_angle,
        0,  // impact count (not tracked)
        0,  // warning count (not tracked)
        bpm > 0 ? bpm : 0,
        !isnan(bodyTemp) ? bodyTemp : 0.0f,
        data.accelX,
        data.accelY,
        data.accelZ,
        fall_event.movement_variance
      );
      
      // Display packet contents
      Serial.println("\n📦 PACKET CONTENTS:");
      Serial.println("  ┌─────────────────────────────────────────┐");
      Serial.print("  │ Packet Type:       0x");
      Serial.print(payload[0], HEX);
      Serial.println(" (Fall Event)        │");
      Serial.print("  │ Packet Size:       ");
      Serial.print(len);
      Serial.println(" bytes                   │");
      Serial.print("  │ Timestamp:         ");
      Serial.print(fall_event.timestamp);
      Serial.println(" ms                │");
      Serial.println("  ├─────────────────────────────────────────┤");
      Serial.print("  │ Jerk Magnitude:    ");
      Serial.print(fall_event.jerk_magnitude, 0);
      Serial.println(" m/s³        │");
      Serial.print("  │ SVM Value:         ");
      Serial.print(fall_event.svm_value, 2);
      Serial.println(" g                 │");
      Serial.print("  │ Angular Velocity:  ");
      Serial.print(fall_event.angular_velocity, 1);
      Serial.println(" °/s           │");
      Serial.print("  │ Pitch Angle:       ");
      Serial.print(fall_event.pitch_angle, 1);
      Serial.println("°                  │");
      Serial.print("  │ Roll Angle:        ");
      Serial.print(fall_event.roll_angle, 1);
      Serial.println("°                  │");
      Serial.println("  ├─────────────────────────────────────────┤");
      Serial.print("  │ Heart Rate:        ");
      Serial.print(bpm);
      Serial.println(" BPM                   │");
      Serial.print("  │ Body Temp:         ");
      Serial.print(bodyTemp, 1);
      Serial.println(" °C                │");
      Serial.print("  │ Movement Variance: ");
      Serial.print(fall_event.movement_variance, 4);
      Serial.println("         │");
      Serial.println("  └─────────────────────────────────────────┘");
      
      // Display raw hex data (first 20 bytes)
      Serial.print("\n  📋 Hex Data (first 20 bytes): ");
      for (int i = 0; i < min(20, len); i++) {
        if (payload[i] < 0x10) Serial.print("0");
        Serial.print(payload[i], HEX);
        Serial.print(" ");
      }
      if (len > 20) Serial.print("...");
      Serial.println();
      
      Serial.println("\n📡 TRANSMISSION STATUS:");
      Serial.print("  → Sending via LoRa...");
      
      unsigned long txStart = millis();
      bool success = loraComm.sendUplink(3, payload, len, true);
      unsigned long txDuration = millis() - txStart;
      
      if (success) {
        Serial.println("\n  ✅ SUCCESS!");
        Serial.print("  ⏱️  Transmission time: ");
        Serial.print(txDuration);
        Serial.println(" ms");
        Serial.print("  📊 Frame counter: ");
        Serial.println(loraComm.getFrameCounter());
        Serial.print("  📶 LoRa RSSI: ");
        Serial.print(loraComm.getRSSI());
        Serial.println(" dBm");
        Serial.println("  ℹ️  Fall events are sent immediately when detected");
        fallEventTriggered = true;
      } else {
        Serial.println("\n  ❌ FAILED!");
        Serial.print("  ⏱️  Attempt duration: ");
        Serial.print(txDuration);
        Serial.println(" ms");
        Serial.println("  ⚠️  Will retry on next cycle");
      }
      Serial.println("╚══════════════════════════════════════════════════════════╝\n");
    }
    
    // Reset fall event flag when recovery state reached
    if (fall_event.state == FallDetector::NORMAL || 
        fall_event.state == FallDetector::RECOVERY) {
      fallEventTriggered = false;
    }
    
    // Send real-time monitoring data every 1 minute (Packet Type 0x01)
    if (currentTime - lastRealtimeTxTime >= REALTIME_TX_INTERVAL) {
      Serial.println("\n╔══════════════════════════════════════════════════════════╗");
      Serial.println("║     📡 REALTIME MONITORING TRANSMISSION (Type 0x01)    ║");
      Serial.println("║                    Every 1 minute                       ║");
      Serial.println("╚══════════════════════════════════════════════════════════╝");
      
      uint8_t payload[20];
      int bpm = ecgMonitor.getBPM();
      float bodyTemp = tempSensor.currentTemp;
      float ambientTemp = tempSensor.ambientTemp;
      
      // Check for abnormal conditions
      uint8_t hrStatus = ecgMonitor.checkHeartRate();
      uint8_t tempStatus = tempSensor.checkTempStatus();
      bool hrAbnormal = (hrStatus == 1 || hrStatus == 2);
      bool tempAbnormal = (tempStatus == 3 || tempStatus == 4);
      bool fallAlert = (fall_event.state == FallDetector::FALL_DETECTED || 
                        fall_event.state == FallDetector::DANGEROUS);
      bool noiseAlert = (soundLevel > 100.0f);
      
      // Use max noise level if available, otherwise use current
      float noiseToSend = (maxNoisedB > 0) ? maxNoisedB : soundLevel;
      bool noiseAlertMax = (noiseToSend > 100.0f);
      
      int len = PayloadBuilder::buildRealtimePayload(
        payload,
        bpm > 0 ? bpm : 0,
        !isnan(bodyTemp) ? bodyTemp : 0.0f,
        !isnan(ambientTemp) ? ambientTemp : 0.0f,
        noiseToSend,
        fall_event.state,
        hrAbnormal,
        tempAbnormal,
        fallAlert,
        noiseAlertMax
      );
      
      // Display packet contents
      Serial.println("\n📦 PACKET CONTENTS:");
      Serial.println("  ┌─────────────────────────────────────────┐");
      Serial.print("  │ Packet Type:       0x");
      Serial.print(payload[0], HEX);
      Serial.println(" (Realtime)          │");
      Serial.print("  │ Packet Size:       ");
      Serial.print(len);
      Serial.println(" bytes                    │");
      Serial.println("  ├─────────────────────────────────────────┤");
      Serial.print("  │ Heart Rate:        ");
      Serial.print(bpm);
      Serial.print(" BPM");
      Serial.println(hrAbnormal ? " ⚠️ " : "    " + String("│"));
      Serial.print("  │ Body Temp:         ");
      Serial.print(bodyTemp, 1);
      Serial.print(" °C");
      Serial.println(tempAbnormal ? " ⚠️" : "   " + String("│"));
      Serial.print("  │ Ambient Temp:      ");
      Serial.print(ambientTemp, 1);
      Serial.println(" °C            │");
      Serial.print("  │ Noise Level (Max): ");
      Serial.print(noiseToSend, 0);
      Serial.print(" dB");
      Serial.println(noiseAlertMax ? " ⚠️" : "   " + String("│"));
      Serial.print("  │ Fall State:        ");
      switch(fall_event.state) {
        case FallDetector::NORMAL: Serial.println("Normal             │"); break;
        case FallDetector::WARNING: Serial.println("Warning ⚠️         │"); break;
        case FallDetector::FALL_DETECTED: Serial.println("Fall Detected 🚨   │"); break;
        case FallDetector::DANGEROUS: Serial.println("Dangerous! 🆘      │"); break;
        case FallDetector::RECOVERY: Serial.println("Recovery           │"); break;
      }
      Serial.println("  ├─────────────────────────────────────────┤");
      Serial.print("  │ Alert Flags:       0b");
      uint8_t flags = payload[6];
      for (int i = 7; i >= 0; i--) {
        Serial.print((flags >> i) & 1);
      }
      Serial.println("       │");
      Serial.print("  │   HR Alert:        ");
      Serial.println((flags & 0x01) ? "YES ⚠️             │" : "No                 │");
      Serial.print("  │   Temp Alert:      ");
      Serial.println((flags & 0x02) ? "YES ⚠️             │" : "No                 │");
      Serial.print("  │   Fall Alert:      ");
      Serial.println((flags & 0x04) ? "YES 🚨             │" : "No                 │");
      Serial.print("  │   Noise Alert:     ");
      Serial.println((flags & 0x08) ? "YES ⚠️             │" : "No                 │");
      Serial.println("  └─────────────────────────────────────────┘");
      
      // Display raw hex data
      Serial.print("\n  📋 Hex Data: ");
      for (int i = 0; i < len; i++) {
        if (payload[i] < 0x10) Serial.print("0");
        Serial.print(payload[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      
      Serial.println("\n📡 TRANSMISSION STATUS:");
      Serial.print("  → Sending via LoRa...");
      
      unsigned long txStart = millis();
      bool success = loraComm.sendUplink(1, payload, len, false);
      unsigned long txDuration = millis() - txStart;
      
      if (success) {
        Serial.println("\n  ✅ SUCCESS!");
        Serial.print("  ⏱️  Transmission time: ");
        Serial.print(txDuration);
        Serial.println(" ms");
        Serial.print("  📊 Frame counter: ");
        Serial.println(loraComm.getFrameCounter());
        Serial.print("  📶 LoRa RSSI: ");
        Serial.print(loraComm.getRSSI());
        Serial.println(" dBm");
        Serial.print("  🕒 Next transmission: ");
        Serial.println(formatTimeRemaining(REALTIME_TX_INTERVAL));
        lastRealtimeTxTime = currentTime;
        
        // Reset max noise tracking after successful transmission
        maxNoisedB = 0.0;
        maxNoiseTimestamp = 0;
      } else {
        Serial.println("\n  ❌ FAILED!");
        Serial.print("  ⏱️  Attempt duration: ");
        Serial.print(txDuration);
        Serial.println(" ms");
        Serial.println("  ⚠️  Will retry on next cycle");
      }
      Serial.println("╚══════════════════════════════════════════════════════════╝\n");
    }
    
    // Send ECG data periodically (Packet Type 0x02) - every 5.1 minutes when heart rate stable
    if (currentTime - lastECGTxTime >= ECG_TX_INTERVAL) {
      int bpm = ecgMonitor.getBPM();
      if (bpm > 40 && bpm < 150) {  // Only send when heart rate in reasonable range
        Serial.println("\n╔══════════════════════════════════════════════════════════╗");
        Serial.println("║         📡 ECG DATA TRANSMISSION (Type 0x02)           ║");
        Serial.println("║                 Every 5.1 minutes                       ║");
        Serial.println("╚══════════════════════════════════════════════════════════╝");
        
        uint8_t payload[70];
        uint8_t compressedECG[50];
        uint8_t pqrst[14];
        
        int ecgLen = ecgMonitor.getCompressedECG(compressedECG, 50);
        int pqrstLen = ecgMonitor.getPQRSTData(pqrst);
        
        int len = PayloadBuilder::buildECGPayload(
          payload,
          compressedECG,
          ecgLen,
          pqrst,
          pqrstLen
        );
        
        // Display packet contents
        Serial.println("\n📦 PACKET CONTENTS:");
        Serial.println("  ┌─────────────────────────────────────────┐");
        Serial.print("  │ Packet Type:       0x");
        Serial.print(payload[0], HEX);
        Serial.println(" (ECG Data)          │");
        Serial.print("  │ Packet Size:       ");
        Serial.print(len);
        Serial.println(" bytes                   │");
        Serial.print("  │ Current BPM:       ");
        Serial.print(bpm);
        Serial.println(" BPM                   │");
        Serial.println("  ├─────────────────────────────────────────┤");
        Serial.print("  │ Compressed ECG:    ");
        Serial.print(ecgLen);
        Serial.println(" bytes (25Hz)        │");
        Serial.print("  │ PQRST Features:    ");
        Serial.print(pqrstLen);
        Serial.println(" bytes              │");
        Serial.print("  │ Compression:       100Hz → 25Hz       │");
        Serial.println();
        Serial.print("  │ Encoding:          8-bit differential  │");
        Serial.println();
        Serial.println("  └─────────────────────────────────────────┘");
        
        // Display compressed ECG sample (first 10 bytes)
        Serial.print("\n  📋 Compressed ECG (first 10 bytes): ");
        for (int i = 0; i < min(10, ecgLen); i++) {
          if (compressedECG[i] < 0x10) Serial.print("0");
          Serial.print(compressedECG[i], HEX);
          Serial.print(" ");
        }
        if (ecgLen > 10) Serial.print("...");
        Serial.println();
        
        if (pqrstLen > 0) {
          Serial.print("  📋 PQRST Features: ");
          for (int i = 0; i < min(14, pqrstLen); i++) {
            if (pqrst[i] < 0x10) Serial.print("0");
            Serial.print(pqrst[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }
        
        Serial.println("\n📡 TRANSMISSION STATUS:");
        Serial.print("  → Sending via LoRa...");
        
        unsigned long txStart = millis();
        bool success = loraComm.sendUplink(2, payload, len, false);
        unsigned long txDuration = millis() - txStart;
        
        if (success) {
          Serial.println("\n  ✅ SUCCESS!");
          Serial.print("  ⏱️  Transmission time: ");
          Serial.print(txDuration);
          Serial.println(" ms");
          Serial.print("  📊 Frame counter: ");
          Serial.println(loraComm.getFrameCounter());
          Serial.print("  📶 LoRa RSSI: ");
          Serial.print(loraComm.getRSSI());
          Serial.println(" dBm");
          Serial.print("  📈 Data rate: ");
          Serial.print((len * 8.0 / txDuration * 1000.0), 0);
          Serial.println(" bps");
          Serial.print("  🕒 Next transmission: ");
          Serial.println(formatTimeRemaining(ECG_TX_INTERVAL));
          lastECGTxTime = currentTime;
        } else {
          Serial.println("\n  ❌ FAILED!");
          Serial.print("  ⏱️  Attempt duration: ");
          Serial.print(txDuration);
          Serial.println(" ms");
          Serial.println("  ⚠️  Will retry on next cycle");
        }
        Serial.println("╚══════════════════════════════════════════════════════════╝\n");
      }
    }
  // LoRa transmission complete - continue monitoring
  
  // Wait before next reading
  delay(READ_INTERVAL_MS);
}
