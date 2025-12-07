# When to Use localhost vs Environment Variables

## Summary

This document explains when to use `localhost` vs reading from `.env` files.

## Use localhost (Hardcoded)

### 1. Docker Internal Communication
```yaml
# docker-compose.yml - containers talking to each other
CORS_ORIGIN: ${CORS_ORIGIN}  # ✅ From .env (external access)
DB_HOST: postgres             # ✅ localhost-like (internal Docker network)
```
Containers use service names (like `postgres`, `api`) within Docker network.

### 2. Container Health Checks
```yaml
# docker-compose.yml
healthcheck:
  test: ["CMD", "wget", "http://localhost:3001"]  # ✅ Container checking itself
```
A container checking its own service uses `localhost`.

### 3. Documentation Examples (When Testing from Server)
```bash
# When running curl ON the server itself
curl http://localhost:5000/health  # ✅ OK if on same machine
```

## Use Environment Variables

### 1. External Access Configuration
```bash
# .env - For external clients to access the server
SERVER_IP=192.168.1.137          # ✅ Actual server IP
CORS_ORIGIN=http://192.168.1.137:5001  # ✅ From .env
REACT_APP_API_URL=http://192.168.1.137:5000  # ✅ From .env
```

### 2. Client Applications
```python
# Raspberry Pi accessing backend API
SERVER_IP = os.getenv('SERVER_IP', 'localhost')  # ✅ Read from .env
```

### 3. Test Scripts
```powershell
# test-notifications.ps1
# Load from .env, fallback to localhost
$ServerUrl = "http://${env:SERVER_IP}:5000"  # ✅ Use environment variable
```

### 4. Frontend Configuration
```javascript
// React app
const API_URL = process.env.REACT_APP_API_URL;  # ✅ From .env
```

## Current Implementation

### ✅ Correct Usage

| File | Usage | Why |
|------|-------|-----|
| `docker-compose.yml` | `DB_HOST: postgres` | Internal Docker network |
| `docker-compose.yml` | `CORS_ORIGIN: ${CORS_ORIGIN}` | External access from .env |
| `docker-compose.yml` | `healthcheck: localhost:3001` | Self-check |
| `raspberry-pi/uart_lora_receiver.py` | `os.getenv('SERVER_IP')` | External client |
| `backend/dashboard/src/App.js` | `process.env.REACT_APP_API_URL` | Browser client |

### 📝 Documentation Localhost Usage

Documentation can show `localhost` in examples **with a note**:

```markdown
# Test API (from server machine)
curl http://localhost:5000/health

# Or use environment variable (from any machine)
source load-env.sh
curl http://$SERVER_IP:5000/health
```

## Best Practices

1. **Internal Docker services** → Use service names (`postgres`, `api`)
2. **External clients** → Use `${SERVER_IP}` from `.env`
3. **Self-checks** → Use `localhost`
4. **Documentation** → Show both options with clear context
5. **Scripts** → Load from `.env` with localhost fallback

## Example: Proper Test Script

```powershell
# Load from .env if available
if (Test-Path ".env") {
    . .\Load-Env.ps1
    $url = "http://${env:SERVER_IP}:5000"
} else {
    $url = "http://localhost:5000"
}

Invoke-WebRequest $url/health
```

## Quick Reference

**Need to access from another machine?** → Use `SERVER_IP` from `.env`

**Container needs database?** → Use `DB_HOST: postgres` (service name)

**Container checks itself?** → Use `localhost`

**Documenting for users?** → Show both options with context
