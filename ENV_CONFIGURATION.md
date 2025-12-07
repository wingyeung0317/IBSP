# Environment Configuration Guide

This project uses environment variables to manage sensitive configuration data across all components.

## Quick Setup

### 1. Backend (Docker Services)

```bash
cd backend
cp .env.example .env
nano .env  # Edit SERVER_IP and other settings
```

**Required Variables:**
- `SERVER_IP` - Your server's IP address (e.g., 192.168.1.100)
- `DB_PASSWORD` - Database password (change from default!)
- `TELEGRAM_BOT_TOKEN` - Telegram bot token (if using notifications)
- `TELEGRAM_CHAT_IDS` - Telegram chat IDs (comma-separated)

### 2. Raspberry Pi Gateway

```bash
cd raspberry-pi
cp .env.example .env
nano .env  # Set SERVER_IP
source .env
python3 uart_lora_receiver.py
```

**Required Variables:**
- `SERVER_IP` - Backend server IP
- `SERVER_PORT` - Backend server port (default: 5000)
- `DEVICE_ID` - Gateway identifier (default: Gateway-001)

### 3. ESP32 Devices

Update the following in your ESP32 code (`esp/src/main.cpp`):

```cpp
#define SERVER_HOST "YOUR_SERVER_IP"
#define SERVER_PORT 5000
```

Replace `YOUR_SERVER_IP` with the same IP used in backend `.env`

## File Locations

```
IBSP/
├── backend/
│   ├── .env                    # Backend configuration (DO NOT COMMIT)
│   └── .env.example            # Template (commit this)
├── raspberry-pi/
│   ├── .env                    # Gateway configuration (DO NOT COMMIT)
│   └── .env.example            # Template (commit this)
└── esp/src/main.cpp            # Update SERVER_HOST manually
```

## Security Best Practices

1. **Never commit `.env` files** - They contain sensitive data
2. **Always commit `.env.example` files** - With placeholder values
3. **Use strong passwords** - Change default database password
4. **Restrict file permissions** - `chmod 600 .env` on Linux/Mac
5. **Rotate credentials regularly** - Especially API tokens

## Environment Variables Reference

### Backend (.env)

| Variable | Description | Example |
|----------|-------------|---------|
| `SERVER_IP` | Server IP address | `192.168.1.100` |
| `DB_USER` | Database username | `healthmonitor` |
| `DB_PASSWORD` | Database password | `secure_password` |
| `DB_NAME` | Database name | `health_monitoring` |
| `API_PORT` | API server port | `5000` |
| `DASHBOARD_PORT` | Dashboard port | `5001` |
| `TELEGRAM_BOT_TOKEN` | Telegram bot token | `123456:ABC-DEF...` |
| `TELEGRAM_CHAT_IDS` | Chat IDs (comma-separated) | `123456,789012` |
| `DISCORD_WEBHOOK_URLS` | Discord webhooks | `https://...` |
| `TWILIO_ACCOUNT_SID` | Twilio account SID | `AC...` |
| `TWILIO_AUTH_TOKEN` | Twilio auth token | `...` |

### Raspberry Pi (.env)

| Variable | Description | Example |
|----------|-------------|---------|
| `SERVER_IP` | Backend server IP | `192.168.1.100` |
| `SERVER_PORT` | Backend server port | `5000` |
| `DEVICE_ID` | Gateway identifier | `Gateway-001` |

## Deployment Checklist

- [ ] Copy `.env.example` to `.env` in each component
- [ ] Update `SERVER_IP` in all `.env` files
- [ ] Change default database password
- [ ] Configure notification services (optional)
- [ ] Update ESP32 code with server IP
- [ ] Verify `.env` is in `.gitignore`
- [ ] Test connections: `curl http://SERVER_IP:5000/health`
- [ ] Start services: `docker-compose up -d`

## Troubleshooting

### "Connection refused"
- Check if `SERVER_IP` is correct in all `.env` files
- Verify firewall allows ports 5000 and 5001
- Ensure Docker services are running: `docker-compose ps`

### "Environment variable not found"
- Ensure `.env` file exists (not just `.env.example`)
- Check file has correct format: `KEY=value` (no spaces around `=`)
- For Raspberry Pi, run: `source .env` before Python script
- For backend testing, use helper scripts:
  - Linux/Mac: `source load-env.sh`
  - Windows: `. .\Load-Env.ps1`

### "Permission denied"
- Set correct permissions: `chmod 600 .env`
- Run as correct user (don't use root unless necessary)

### Testing with environment variables

**Linux/Mac:**
```bash
# Load variables
source backend/load-env.sh

# Test API
curl http://$SERVER_IP:$API_PORT/health
```

**Windows PowerShell:**
```powershell
# Load variables
cd backend
. .\Load-Env.ps1

# Test API
Invoke-WebRequest "http://$env:SERVER_IP:$env:API_PORT/health"
```

## Getting Help

1. Check `.env.example` files for correct format
2. Review component-specific README files
3. Ensure all services use the same `SERVER_IP`
4. Check Docker logs: `docker-compose logs -f`
5. Verify network connectivity: `ping SERVER_IP`

## Additional Resources

- [Backend Setup Guide](backend/ENV_SETUP.md)
- [Notification Configuration](backend/NOTIFICATION_SETUP.md)
- [Raspberry Pi Setup](raspberry-pi/README.md)
- [ESP32 Configuration](esp/README.md)
