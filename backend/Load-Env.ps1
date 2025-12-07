# ============================================================================
# Load Environment Variables Helper Script (PowerShell)
# ============================================================================
# This script loads environment variables from .env file
# Usage: . .\Load-Env.ps1   or   source Load-Env.ps1

# Check if .env file exists
if (-not (Test-Path ".env")) {
    Write-Host "⚠️  Warning: .env file not found!" -ForegroundColor Yellow
    Write-Host "   Copy from .env.example: cp .env.example .env" -ForegroundColor Yellow
    return
}

# Load environment variables from .env file
Get-Content ".env" | ForEach-Object {
    if ($_ -match '^([^#][^=]+)=(.+)$') {
        $name = $matches[1].Trim()
        $value = $matches[2].Trim()
        Set-Item -Path "env:$name" -Value $value
        Write-Verbose "Set $name=$value"
    }
}

# Display loaded configuration
Write-Host "✅ Environment variables loaded from .env" -ForegroundColor Green
Write-Host ""
Write-Host "Configuration:" -ForegroundColor Cyan
Write-Host "  SERVER_IP=$env:SERVER_IP" -ForegroundColor White
Write-Host "  API_PORT=$env:API_PORT" -ForegroundColor White
Write-Host "  DASHBOARD_PORT=$env:DASHBOARD_PORT" -ForegroundColor White
Write-Host ""
Write-Host "You can now use these variables in your scripts." -ForegroundColor Yellow
Write-Host "Example: Invoke-WebRequest http://`$env:SERVER_IP`:$env:API_PORT/health" -ForegroundColor Gray
