# ============================================================================
# Environment Variables Setup Guide
# ============================================================================

## Quick Start

1. **Copy the example file:**
   ```bash
   cp .env.example .env
   ```

2. **Update `.env` with your values:**
   - Replace `YOUR_SERVER_IP` with your actual server IP address
   - Update database password
   - Add your Telegram bot token and chat ID (if using notifications)
   - Configure Discord/WhatsApp credentials (if needed)

3. **Load environment variables (optional for testing):**
   
   **Linux/Mac:**
   ```bash
   source load-env.sh
   ```
   
   **Windows PowerShell:**
   ```powershell
   . .\Load-Env.ps1
   ```

4. **Start the services:**
   ```bash
   docker-compose up -d
   ```

## Important Notes

- **Never commit `.env` file to GitHub!** It contains sensitive information.
- The `.env` file is already listed in `.gitignore`
- Only commit `.env.example` with placeholder values

## Configuration Variables

### Server Network
- `SERVER_IP`: Your server's IP address (e.g., 192.168.1.100)

### Database
- `DB_USER`: Database username
- `DB_PASSWORD`: Database password (change from default!)
- `DB_NAME`: Database name

### API & Dashboard Ports
- `API_PORT`: API server port (default: 5000)
- `DASHBOARD_PORT`: Dashboard port (default: 5001)

### Notifications
- Configure Telegram, Discord, or WhatsApp by setting `*_ENABLED=true`
- Add corresponding tokens/webhooks/credentials

## ESP32 Configuration

Update the following in `esp/src/main.cpp`:
```cpp
#define SERVER_HOST "YOUR_SERVER_IP"
#define SERVER_PORT 5000
```

Replace `YOUR_SERVER_IP` with the same IP used in `.env`
