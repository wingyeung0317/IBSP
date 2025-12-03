const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const morgan = require('morgan');
const { Pool } = require('pg');

const app = express();
const PORT = process.env.PORT || 3000;

// Store tracking start time for each device (in-memory)
// Format: { 'device_id': timestamp }
const deviceTrackingStartTime = {};

// PostgreSQL connection pool
const pool = new Pool({
  host: process.env.DB_HOST || 'localhost',
  port: process.env.DB_PORT || 5432,
  user: process.env.DB_USER || 'healthmonitor',
  password: process.env.DB_PASSWORD || 'healthpass123',
  database: process.env.DB_NAME || 'health_monitoring',
  max: 20,
  idleTimeoutMillis: 30000,
  connectionTimeoutMillis: 2000,
});

// Test database connection
pool.query('SELECT NOW()', (err, res) => {
  if (err) {
    console.error('❌ Database connection failed:', err);
  } else {
    console.log('✅ Database connected successfully');
  }
});

// Middleware
app.use(cors({ origin: process.env.CORS_ORIGIN || '*' }));
app.use(bodyParser.json({ limit: '10mb' }));
app.use(bodyParser.urlencoded({ extended: true }));
app.use(morgan('combined'));

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Decode temperature from int8 (0-255) to Celsius (-20°C to 80°C)
 */
function decodeTemperature(encoded) {
  return ((encoded / 255.0) * 100.0) - 20.0;
}

/**
 * Parse binary packet data
 */
function parsePacket(base64Data, packetType) {
  const buffer = Buffer.from(base64Data, 'base64');
  
  if (packetType === 1) {
    // Real-time data packet (10 bytes)
    if (buffer.length < 10) return null;
    
    return {
      packet_type: buffer.readUInt8(0),
      heart_rate: buffer.readUInt8(1),
      body_temperature: decodeTemperature(buffer.readUInt8(2)),
      ambient_temperature: decodeTemperature(buffer.readUInt8(3)),
      noise_level: buffer.readUInt8(4),
      fall_state: buffer.readUInt8(5),
      alert_flags: buffer.readUInt8(6),
      rssi: buffer.readInt16LE(7),
      snr: buffer.readInt8(9)
    };
  } else if (packetType === 2) {
    // ECG data packet (65 bytes)
    if (buffer.length < 65) return null;
    
    const compressed_ecg = buffer.slice(1, 51);
    const pqrst_timestamp = buffer.readUInt16LE(51);
    const p_amp = buffer.readInt16LE(53);
    const q_amp = buffer.readInt16LE(55);
    const r_amp = buffer.readInt16LE(57);
    const s_amp = buffer.readInt16LE(59);
    const t_amp = buffer.readInt16LE(61);
    const qrs_width = buffer.readUInt8(63);
    const qt_interval = buffer.readUInt8(64);
    
    return {
      packet_type: buffer.readUInt8(0),
      compressed_ecg,
      pqrst_timestamp,
      p_amp,
      q_amp,
      r_amp,
      s_amp,
      t_amp,
      qrs_width,
      qt_interval
    };
  } else if (packetType === 3) {
    // Fall event packet (45 bytes)
    if (buffer.length < 45) return null;
    
    return {
      packet_type: buffer.readUInt8(0),
      timestamp: buffer.readUInt32LE(1),
      jerk_magnitude: buffer.readFloatLE(5),
      svm_value: buffer.readFloatLE(9),
      angular_velocity: buffer.readFloatLE(13),
      pitch_angle: buffer.readFloatLE(17),
      roll_angle: buffer.readFloatLE(21),
      impact_count: buffer.readUInt8(25),
      warning_count: buffer.readUInt8(26),
      heart_rate: buffer.readUInt8(27),
      body_temperature: decodeTemperature(buffer.readUInt8(28)),
      accel_x: buffer.readFloatLE(29),
      accel_y: buffer.readFloatLE(33),
      accel_z: buffer.readFloatLE(37),
      movement_variance: buffer.readFloatLE(41)
    };
  }
  
  return null;
}

// ============================================================================
// API ENDPOINTS
// ============================================================================

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Receive sensor data from ESP32
app.post('/api/sensor-data', async (req, res) => {
  const { device_id, packet_type, data, timestamp, frame_counter, rssi } = req.body;
  
  console.log(`📡 Received packet from ${device_id}: Type ${packet_type}, Frame ${frame_counter}`);
  
  try {
    // Parse the base64 encoded data
    const parsedData = parsePacket(data, packet_type);
    
    if (!parsedData) {
      return res.status(400).json({ error: 'Invalid packet data' });
    }
    
    // Ensure device exists
    await pool.query(
      `INSERT INTO devices (device_id, last_seen) 
       VALUES ($1, CURRENT_TIMESTAMP)
       ON CONFLICT (device_id) DO UPDATE SET last_seen = CURRENT_TIMESTAMP`,
      [device_id]
    );
    
    // Log raw packet
    await pool.query(
      `INSERT INTO packet_log (device_id, packet_type, raw_data, data_length, frame_counter, rssi)
       VALUES ($1, $2, $3, $4, $5, $6)`,
      [device_id, packet_type, Buffer.from(data, 'base64'), Buffer.from(data, 'base64').length, frame_counter, rssi]
    );
    
    // Store data based on packet type
    if (packet_type === 1) {
      // Real-time monitoring data
      const alertFlags = parsedData.alert_flags;
      await pool.query(
        `INSERT INTO realtime_data 
         (device_id, heart_rate, body_temperature, ambient_temperature, noise_level, fall_state,
          alert_hr_abnormal, alert_temp_abnormal, alert_fall, alert_noise, rssi, frame_counter)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)`,
        [
          device_id,
          parsedData.heart_rate,
          parsedData.body_temperature,
          parsedData.ambient_temperature,
          parsedData.noise_level,
          parsedData.fall_state,
          (alertFlags & 0x01) !== 0,
          (alertFlags & 0x02) !== 0,
          (alertFlags & 0x04) !== 0,
          (alertFlags & 0x08) !== 0,
          parsedData.rssi,
          frame_counter
        ]
      );
      
      console.log(`  ✅ Stored real-time data: HR=${parsedData.heart_rate}, Temp=${parsedData.body_temperature.toFixed(1)}°C`);
      
    } else if (packet_type === 2) {
      // ECG data
      await pool.query(
        `INSERT INTO ecg_data 
         (device_id, compressed_ecg, p_amplitude, q_amplitude, r_amplitude, s_amplitude, t_amplitude,
          qrs_width, qt_interval, pqrst_timestamp)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)`,
        [
          device_id,
          parsedData.compressed_ecg,
          parsedData.p_amp,
          parsedData.q_amp,
          parsedData.r_amp,
          parsedData.s_amp,
          parsedData.t_amp,
          parsedData.qrs_width,
          parsedData.qt_interval,
          parsedData.pqrst_timestamp
        ]
      );
      
      console.log(`  ✅ Stored ECG data: R-peak=${parsedData.r_amp}, QRS=${parsedData.qrs_width}ms`);
      
    } else if (packet_type === 3) {
      // Fall event
      await pool.query(
        `INSERT INTO fall_events 
         (device_id, event_timestamp, jerk_magnitude, svm_value, angular_velocity, pitch_angle, roll_angle,
          impact_count, warning_count, heart_rate, body_temperature, accel_x, accel_y, accel_z, movement_variance)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)`,
        [
          device_id,
          parsedData.timestamp,
          parsedData.jerk_magnitude,
          parsedData.svm_value,
          parsedData.angular_velocity,
          parsedData.pitch_angle,
          parsedData.roll_angle,
          parsedData.impact_count,
          parsedData.warning_count,
          parsedData.heart_rate,
          parsedData.body_temperature,
          parsedData.accel_x,
          parsedData.accel_y,
          parsedData.accel_z,
          parsedData.movement_variance
        ]
      );
      
      console.log(`  🚨 FALL EVENT: Jerk=${parsedData.jerk_magnitude.toFixed(0)}, SVM=${parsedData.svm_value.toFixed(2)}g`);
    }
    
    res.json({ status: 'success', message: 'Data stored successfully' });
    
  } catch (error) {
    console.error('❌ Error processing data:', error);
    res.status(500).json({ error: 'Internal server error', details: error.message });
  }
});

// Get latest vitals for all devices
app.get('/api/vitals/latest', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM latest_vitals');
    res.json(result.rows);
  } catch (error) {
    console.error('Error fetching latest vitals:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Get real-time data for a specific device
app.get('/api/realtime/:device_id', async (req, res) => {
  const { device_id } = req.params;
  const limit = req.query.limit || 100;
  
  try {
    const startTime = deviceTrackingStartTime[device_id];
    let query, params;
    
    if (startTime) {
      query = `SELECT * FROM realtime_data 
               WHERE device_id = $1 AND timestamp >= $2
               ORDER BY timestamp DESC 
               LIMIT $3`;
      params = [device_id, startTime, limit];
    } else {
      query = `SELECT * FROM realtime_data 
               WHERE device_id = $1 
               ORDER BY timestamp DESC 
               LIMIT $2`;
      params = [device_id, limit];
    }
    
    const result = await pool.query(query, params);
    res.json(result.rows);
  } catch (error) {
    console.error('Error fetching realtime data:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Get ECG data for a specific device
app.get('/api/ecg/:device_id', async (req, res) => {
  const { device_id } = req.params;
  const limit = req.query.limit || 50;
  
  try {
    const startTime = deviceTrackingStartTime[device_id];
    let query, params;
    
    if (startTime) {
      query = `SELECT * FROM ecg_data 
               WHERE device_id = $1 AND timestamp >= $2
               ORDER BY timestamp DESC 
               LIMIT $3`;
      params = [device_id, startTime, limit];
    } else {
      query = `SELECT * FROM ecg_data 
               WHERE device_id = $1 
               ORDER BY timestamp DESC 
               LIMIT $2`;
      params = [device_id, limit];
    }
    
    const result = await pool.query(query, params);
    res.json(result.rows);
  } catch (error) {
    console.error('Error fetching ECG data:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Get fall events for a specific device
app.get('/api/falls/:device_id', async (req, res) => {
  const { device_id } = req.params;
  const limit = req.query.limit || 50;
  
  try {
    const startTime = deviceTrackingStartTime[device_id];
    let query, params;
    
    if (startTime) {
      query = `SELECT * FROM fall_events 
               WHERE device_id = $1 AND timestamp >= $2
               ORDER BY timestamp DESC 
               LIMIT $3`;
      params = [device_id, startTime, limit];
    } else {
      query = `SELECT * FROM fall_events 
               WHERE device_id = $1 
               ORDER BY timestamp DESC 
               LIMIT $2`;
      params = [device_id, limit];
    }
    
    const result = await pool.query(query, params);
    res.json(result.rows);
  } catch (error) {
    console.error('Error fetching fall events:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Get pending fall alerts
app.get('/api/falls/alerts/pending', async (req, res) => {
  try {
    // Get all pending alerts and filter by tracking start time if exists
    const result = await pool.query('SELECT * FROM pending_fall_alerts');
    
    // Filter by device tracking start time
    const filteredAlerts = result.rows.filter(alert => {
      const startTime = deviceTrackingStartTime[alert.device_id];
      if (!startTime) return true;
      return new Date(alert.timestamp) >= new Date(startTime);
    });
    
    res.json(filteredAlerts);
  } catch (error) {
    console.error('Error fetching pending alerts:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Update fall event response status
app.patch('/api/falls/:fall_id/status', async (req, res) => {
  const { fall_id } = req.params;
  const { status, notes } = req.body;
  
  try {
    await pool.query(
      `UPDATE fall_events 
       SET response_status = $1, notes = $2 
       WHERE id = $3`,
      [status, notes, fall_id]
    );
    res.json({ status: 'success', message: 'Status updated' });
  } catch (error) {
    console.error('Error updating fall status:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Acknowledge fall alert
app.put('/api/falls/:fall_id/acknowledge', async (req, res) => {
  const { fall_id } = req.params;
  
  try {
    await pool.query(
      `UPDATE fall_events 
       SET response_status = 'acknowledged', response_time = CURRENT_TIMESTAMP
       WHERE id = $1`,
      [fall_id]
    );
    res.json({ status: 'success', message: 'Alert acknowledged' });
  } catch (error) {
    console.error('Error acknowledging fall alert:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// Set tracking start time (reset panel view without deleting data)
app.post('/api/tracking/reset/:device_id', async (req, res) => {
  const { device_id } = req.params;
  
  try {
    const now = new Date().toISOString();
    deviceTrackingStartTime[device_id] = now;
    
    console.log(`🔄 Reset tracking start time for ${device_id} to ${now}`);
    
    res.json({ 
      status: 'success', 
      message: `Tracking reset for device ${device_id}. Dashboard will show data from now onwards.`,
      tracking_start_time: now
    });
    
  } catch (error) {
    console.error('Error resetting tracking time:', error);
    res.status(500).json({ error: 'Internal server error', details: error.message });
  }
});

// Get tracking start time for a device
app.get('/api/tracking/:device_id', async (req, res) => {
  const { device_id } = req.params;
  
  try {
    const startTime = deviceTrackingStartTime[device_id];
    
    res.json({ 
      device_id,
      tracking_start_time: startTime || null,
      is_tracking_filtered: !!startTime
    });
    
  } catch (error) {
    console.error('Error getting tracking time:', error);
    res.status(500).json({ error: 'Internal server error', details: error.message });
  }
});

// Clear tracking filter (show all historical data again)
app.delete('/api/tracking/reset/:device_id', async (req, res) => {
  const { device_id } = req.params;
  
  try {
    delete deviceTrackingStartTime[device_id];
    
    console.log(`♻️  Removed tracking filter for ${device_id}. Showing all historical data.`);
    
    res.json({ 
      status: 'success', 
      message: `Tracking filter removed for device ${device_id}. Dashboard will show all historical data.`
    });
    
  } catch (error) {
    console.error('Error clearing tracking filter:', error);
    res.status(500).json({ error: 'Internal server error', details: error.message });
  }
});

// Get statistics
app.get('/api/stats/:device_id', async (req, res) => {
  const { device_id } = req.params;
  
  try {
    const stats = await pool.query(
      `SELECT 
        COUNT(*) as total_readings,
        AVG(heart_rate) as avg_heart_rate,
        AVG(body_temperature) as avg_body_temp,
        MAX(noise_level) as max_noise,
        COUNT(CASE WHEN alert_fall THEN 1 END) as fall_alerts
       FROM realtime_data 
       WHERE device_id = $1 AND timestamp > NOW() - INTERVAL '24 hours'`,
      [device_id]
    );
    
    const fallCount = await pool.query(
      `SELECT COUNT(*) as total_falls 
       FROM fall_events 
       WHERE device_id = $1`,
      [device_id]
    );
    
    res.json({
      ...stats.rows[0],
      total_falls: fallCount.rows[0].total_falls
    });
  } catch (error) {
    console.error('Error fetching stats:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// List all devices
app.get('/api/devices', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM devices ORDER BY last_seen DESC');
    res.json(result.rows);
  } catch (error) {
    console.error('Error fetching devices:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// 404 handler
app.use((req, res) => {
  res.status(404).json({ error: 'Endpoint not found' });
});

// Error handler
app.use((err, req, res, next) => {
  console.error('Unhandled error:', err);
  res.status(500).json({ error: 'Internal server error' });
});

// Start server
app.listen(PORT, '0.0.0.0', () => {
  console.log(`\n🚀 Health Monitor API Server`);
  console.log(`📡 Listening on port ${PORT}`);
  console.log(`🗄️  Database: ${process.env.DB_HOST || 'localhost'}:${process.env.DB_PORT || 5432}`);
  console.log(`\n📋 Available endpoints:`);
  console.log(`   POST   /api/sensor-data - Receive data from ESP32`);
  console.log(`   GET    /api/vitals/latest - Get latest vitals`);
  console.log(`   GET    /api/realtime/:device_id - Get real-time data`);
  console.log(`   GET    /api/ecg/:device_id - Get ECG data`);
  console.log(`   GET    /api/falls/:device_id - Get fall events`);
  console.log(`   GET    /api/devices - List all devices`);
  console.log(`   POST   /api/tracking/reset/:device_id - Reset tracking start time`);
  console.log(`   GET    /api/tracking/:device_id - Get tracking info`);
  console.log(`   DELETE /api/tracking/reset/:device_id - Clear tracking filter`);
  console.log(`\n✨ Server ready!\n`);
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM received, closing server...');
  pool.end();
  process.exit(0);
});
