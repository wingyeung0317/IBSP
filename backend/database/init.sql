-- Health Monitoring System Database Schema

-- Device Information Table
CREATE TABLE IF NOT EXISTS devices (
    device_id VARCHAR(50) PRIMARY KEY,
    device_name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Real-time Monitoring Data (Packet Type 0x01)
CREATE TABLE IF NOT EXISTS realtime_data (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) REFERENCES devices(device_id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    heart_rate INTEGER CHECK (heart_rate >= 0 AND heart_rate <= 255),
    body_temperature NUMERIC(5,2),
    ambient_temperature NUMERIC(5,2),
    noise_level INTEGER CHECK (noise_level >= 0 AND noise_level <= 255),
    fall_state INTEGER CHECK (fall_state >= 0 AND fall_state <= 4),
    alert_hr_abnormal BOOLEAN DEFAULT FALSE,
    alert_temp_abnormal BOOLEAN DEFAULT FALSE,
    alert_fall BOOLEAN DEFAULT FALSE,
    alert_noise BOOLEAN DEFAULT FALSE,
    rssi INTEGER,
    frame_counter INTEGER
);

-- ECG Data (Packet Type 0x02)
CREATE TABLE IF NOT EXISTS ecg_data (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) REFERENCES devices(device_id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    compressed_ecg BYTEA,
    p_amplitude INTEGER,
    q_amplitude INTEGER,
    r_amplitude INTEGER,
    s_amplitude INTEGER,
    t_amplitude INTEGER,
    qrs_width INTEGER,
    qt_interval INTEGER,
    pqrst_timestamp INTEGER
);

-- Fall Events (Packet Type 0x03)
CREATE TABLE IF NOT EXISTS fall_events (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) REFERENCES devices(device_id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    event_timestamp BIGINT,
    jerk_magnitude REAL,
    svm_value REAL,
    angular_velocity REAL,
    pitch_angle REAL,
    roll_angle REAL,
    impact_count INTEGER,
    warning_count INTEGER,
    heart_rate INTEGER,
    body_temperature NUMERIC(5,2),
    accel_x REAL,
    accel_y REAL,
    accel_z REAL,
    movement_variance REAL,
    response_status VARCHAR(50) DEFAULT 'pending',
    notes TEXT
);

-- Raw Packet Log (for debugging)
CREATE TABLE IF NOT EXISTS packet_log (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) REFERENCES devices(device_id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    packet_type INTEGER,
    raw_data BYTEA,
    data_length INTEGER,
    frame_counter INTEGER,
    rssi INTEGER
);

-- Create indexes for better query performance
CREATE INDEX idx_realtime_device_timestamp ON realtime_data(device_id, timestamp DESC);
CREATE INDEX idx_ecg_device_timestamp ON ecg_data(device_id, timestamp DESC);
CREATE INDEX idx_fall_device_timestamp ON fall_events(device_id, timestamp DESC);
CREATE INDEX idx_fall_status ON fall_events(response_status);
CREATE INDEX idx_packet_device_type ON packet_log(device_id, packet_type, timestamp DESC);

-- Create views for easy querying
CREATE OR REPLACE VIEW latest_vitals AS
SELECT 
    d.device_id,
    d.device_name,
    r.timestamp,
    r.heart_rate,
    r.body_temperature,
    r.ambient_temperature,
    r.noise_level,
    r.fall_state,
    r.alert_hr_abnormal,
    r.alert_temp_abnormal,
    r.alert_fall,
    r.alert_noise
FROM devices d
LEFT JOIN LATERAL (
    SELECT * FROM realtime_data 
    WHERE device_id = d.device_id 
    ORDER BY timestamp DESC 
    LIMIT 1
) r ON true;

CREATE OR REPLACE VIEW pending_fall_alerts AS
SELECT 
    f.*,
    d.device_name,
    EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - f.timestamp)) as seconds_ago
FROM fall_events f
JOIN devices d ON f.device_id = d.device_id
WHERE f.response_status = 'pending'
ORDER BY f.timestamp DESC;

-- Insert default device (ESP32-001)
INSERT INTO devices (device_id, device_name) 
VALUES ('ESP32-001', 'Health Monitor Device 1')
ON CONFLICT (device_id) DO NOTHING;

-- Function to decode temperature from int8 to Celsius
CREATE OR REPLACE FUNCTION decode_temperature(encoded_temp INTEGER)
RETURNS NUMERIC(5,2) AS $$
BEGIN
    -- Map 0-255 back to -20°C to 80°C
    RETURN ((encoded_temp::NUMERIC / 255.0) * 100.0) - 20.0;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

COMMENT ON TABLE devices IS 'ESP32 device registration';
COMMENT ON TABLE realtime_data IS 'Real-time monitoring data (Packet Type 0x01)';
COMMENT ON TABLE ecg_data IS 'ECG waveform data (Packet Type 0x02)';
COMMENT ON TABLE fall_events IS 'Fall detection events (Packet Type 0x03)';
COMMENT ON TABLE packet_log IS 'Raw packet log for debugging';
