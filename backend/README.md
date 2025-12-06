# Health Monitoring Dashboard

Complete Docker-based health monitoring system for ESP32 IoT devices with fall detection, ECG monitoring, temperature sensing, and noise level tracking.

## System Architecture

```
ESP32 Device (WiFi/LoRa) → API Server (Node.js) → PostgreSQL Database
                                                ↓
                                         React Dashboard
                                                ↓
                                    Notification Services
                                    (Telegram/Discord/WhatsApp)
```

## Features

### 📡 Data Collection
- **Fall Detection**: Multi-stage fall detection with jerk, SVM, angular velocity, and posture analysis
- **ECG Monitoring**: Heart rate, PQRST wave analysis, compressed ECG waveforms
- **Temperature**: Body and ambient temperature monitoring via MLX90614 IR sensor
- **Noise Level**: Environmental noise monitoring (dB) via MAX4466 microphone

### 📊 Dashboard Features
- Real-time vital signs display (heart rate, temperature, noise level)
- Historical data charts with time-series visualization
- Fall alert notifications
- Multi-device support
- Auto-refresh every 5 seconds

### 🔔 Alert Notifications
- **Telegram Bot**: Instant messages to personal or group chats
- **Discord Webhooks**: Rich embedded alerts to Discord channels
- **WhatsApp**: SMS-style alerts via Twilio API
- Configurable alert thresholds for all vital signs
- Automatic cooldown to prevent notification spam
- Real-time alert status monitoring

### 🗄️ Database
- PostgreSQL with optimized schema for sensor data
- Separate tables for real-time data, ECG, and fall events
- Indexes for fast queries
- Views for common queries

## Quick Start

### Prerequisites
- Docker and Docker Compose installed
- ESP32 device with sensors (optional, for testing)

### 1. Clone/Download Files

```bash
cd health-monitor-dashboard
```

### 2. Configuration

Edit the ESP32 code `main.cpp` to set your WiFi credentials and server URL:

```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourPassword";
const char* SERVER_URL = "http://YOUR_SERVER_IP:5000/api/sensor-data";
```

### 3. Configure Notifications (Optional)

The system supports multiple notification channels for alerts. See **[NOTIFICATION_SETUP.md](NOTIFICATION_SETUP.md)** for detailed instructions.

**Quick setup using PowerShell:**
```powershell
.\setup-notifications.ps1
```

**Or manually edit `docker-compose.yml`:**
```yaml
# Enable Telegram notifications
TELEGRAM_ENABLED: "true"
TELEGRAM_BOT_TOKEN: "your_bot_token"
TELEGRAM_CHAT_IDS: "123456789,-987654321"

# Enable Discord notifications
DISCORD_ENABLED: "true"
DISCORD_WEBHOOK_URLS: "https://discord.com/api/webhooks/..."

# Enable WhatsApp notifications (via Twilio)
WHATSAPP_ENABLED: "true"
TWILIO_ACCOUNT_SID: "ACxxxx..."
TWILIO_AUTH_TOKEN: "xxxx..."
TWILIO_WHATSAPP_FROM: "+14155238886"
WHATSAPP_TO_NUMBERS: "+886912345678"
```

### 4. Start the System

```bash
docker-compose up -d
```

This will start:
- PostgreSQL database on port 5432
- API server on port 5000
- React dashboard on port 5001

### 4. Access the Dashboard

Open your browser and navigate to:
```
http://localhost:5001
```

## API Endpoints

### POST /api/sensor-data
Receive sensor data from ESP32 devices.

**Request Body:**
```json
{
  "device_id": "ESP32-001",
  "packet_type": 1,
  "data": "base64_encoded_binary_data",
  "timestamp": 1234567890,
  "frame_counter": 42,
  "rssi": -65
}
```

### GET /api/vitals/latest
Get latest vitals for all devices.

### GET /api/realtime/:device_id?limit=100
Get real-time monitoring data history.

### GET /api/ecg/:device_id?limit=50
Get ECG data.

### GET /api/falls/:device_id?limit=50
Get fall events.

### GET /api/falls/alerts/pending
Get pending fall alerts.

### GET /api/devices
List all registered devices.

### GET /api/notifications/status
Get notification service configuration status.

**Response:**
```json
{
  "telegram": {
    "enabled": true,
    "configured": true,
    "chatCount": 2
  },
  "discord": {
    "enabled": true,
    "configured": true,
    "webhookCount": 1
  },
  "whatsapp": {
    "enabled": false,
    "configured": false,
    "recipientCount": 0
  }
}
```

### POST /api/notifications/test
Send a test notification to all configured channels.

**Request Body:**
```json
{
  "channel": "telegram",
  "deviceId": "TEST-DEVICE"
}
```

## Notification System

The system automatically sends alerts when:
- 🚨 **Fall detected** - Immediate notification with fall details
- 💓 **Heart rate abnormal** - Outside configured thresholds (50-120 bpm default)
- 🌡️ **Body temperature abnormal** - Outside configured range (36-38°C default)
- 🔊 **High noise level** - Above threshold (200/255 default)

### Alert Cooldown
- Alerts have a 5-minute cooldown period to prevent spam
- Fall events are always sent (no cooldown)
- Multiple channels can be enabled simultaneously

### Supported Platforms
- **Telegram**: Personal or group chat notifications with Markdown formatting
- **Discord**: Rich embedded messages with color-coded alerts
- **WhatsApp**: Text messages via Twilio (requires paid account for production use)

For detailed setup instructions, see **[NOTIFICATION_SETUP.md](NOTIFICATION_SETUP.md)**.

## Data Packet Formats

### Packet Type 0x01 - Real-time Monitoring (10 bytes)
```
[0] Packet type: 0x01
[1] Heart rate (BPM): uint8
[2] Body temperature: int8 (encoded)
[3] Ambient temperature: int8 (encoded)
[4] Noise level (dB): uint8
[5] Fall state: uint8 (0-4)
[6] Alert flags: uint8 (bits)
[7-8] RSSI: int16
[9] SNR: int8
```

### Packet Type 0x02 - ECG Data (65 bytes)
```
[0] Packet type: 0x02
[1-50] Compressed ECG (25Hz, 2 seconds)
[51-64] PQRST features (timestamp + 5 amplitudes + 2 intervals)
```

### Packet Type 0x03 - Fall Event (45 bytes)
```
[0] Packet type: 0x03
[1-4] Timestamp: uint32
[5-8] Jerk magnitude: float32
[9-12] SVM value: float32
[13-16] Angular velocity: float32
[17-20] Pitch angle: float32
[21-24] Roll angle: float32
[25-44] Additional metrics and vital signs
```

## Database Schema

### Tables
- `devices` - Device registration
- `realtime_data` - Real-time monitoring data
- `ecg_data` - ECG waveforms and PQRST features
- `fall_events` - Fall detection events
- `packet_log` - Raw packet log for debugging

### Views
- `latest_vitals` - Latest vital signs for all devices
- `pending_fall_alerts` - Unresolved fall alerts

## Development

### Run in Development Mode

**API Server:**
```bash
cd api
npm install
npm run dev
```

**Dashboard:**
```bash
cd dashboard
npm install
npm start
```

**Database:**
```bash
docker run -d \
  --name health-monitor-db \
  -e POSTGRES_USER=healthmonitor \
  -e POSTGRES_PASSWORD=healthpass123 \
  -e POSTGRES_DB=health_monitoring \
  -p 5432:5432 \
  postgres:15-alpine
```

## Testing Without ESP32

You can test the system by sending mock data to the API:

```bash
curl -X POST http://localhost:5000/api/sensor-data \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32-001",
    "packet_type": 1,
    "data": "AUxAIhIyUAEAAAA=",
    "timestamp": 1234567890,
    "frame_counter": 1,
    "rssi": -65
  }'
```

## Production Deployment

### Using Docker Compose (Recommended)

1. Update environment variables in `docker-compose.yml`
2. Use strong passwords
3. Configure SSL/TLS certificates
4. Set up reverse proxy (nginx) if needed

### Manual Deployment

1. Deploy PostgreSQL database
2. Build and deploy API server
3. Build and deploy React dashboard
4. Configure firewall rules
5. Set up domain and SSL

## Monitoring

### Check Service Status
```bash
docker-compose ps
```

### View Logs
```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f api
docker-compose logs -f dashboard
docker-compose logs -f postgres
```

### Database Access
```bash
docker-compose exec postgres psql -U healthmonitor -d health_monitoring
```

## Troubleshooting

### Dashboard shows "Failed to fetch data"
- Check if API server is running: `docker-compose ps`
- Check API logs: `docker-compose logs api`
- Verify network connectivity

### ESP32 cannot connect to API
- Ensure SERVER_URL is correct and accessible from ESP32
- Check if API server port (5000) is open
- Verify WiFi credentials

### Database connection errors
- Check if PostgreSQL is running
- Verify database credentials
- Check network connectivity between services

## Security Considerations

1. **Change Default Passwords**: Update PostgreSQL password in `docker-compose.yml`
2. **Use HTTPS**: Configure SSL certificates for production
3. **API Authentication**: Add authentication middleware (JWT, API keys)
4. **Network Security**: Use Docker networks and firewall rules
5. **Data Encryption**: Encrypt sensitive health data at rest

## License

MIT License - See LICENSE file for details

## Contributing

Contributions welcome! Please open an issue or submit a pull request.

## Support

For issues and questions, please open a GitHub issue or contact the development team.
