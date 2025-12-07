# Test Notification Services
# ==========================
# This script tests all configured notification channels

param(
    [string]$ServerUrl
)

# Load SERVER_IP from .env if not provided
if (-not $ServerUrl) {
    if (Test-Path ".env") {
        $envContent = Get-Content ".env" | Where-Object { $_ -match '^SERVER_IP=' }
        if ($envContent) {
            $serverIp = ($envContent -split '=')[1]
            $ServerUrl = "http://${serverIp}:5000"
            Write-Host "Loaded SERVER_IP from .env: $serverIp" -ForegroundColor Green
        } else {
            $ServerUrl = "http://localhost:5000"
            Write-Host "SERVER_IP not found in .env, using localhost" -ForegroundColor Yellow
        }
    } else {
        $ServerUrl = "http://localhost:5000"
        Write-Host ".env file not found, using localhost" -ForegroundColor Yellow
    }
}

Write-Host "`n===========================================================" -ForegroundColor Cyan
Write-Host "  Notification Services Test" -ForegroundColor Cyan
Write-Host "===========================================================`n" -ForegroundColor Cyan

Write-Host "Server URL: $ServerUrl`n" -ForegroundColor Yellow

# Test 1: Check service status
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White
Write-Host "Test 1: Checking Notification Service Status" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White

try {
    $statusUrl = "$ServerUrl/api/notifications/status"
    Write-Host "Requesting: $statusUrl" -ForegroundColor Gray
    
    $status = Invoke-RestMethod -Uri $statusUrl -Method Get -ContentType "application/json"
    
    Write-Host "`nService Status:" -ForegroundColor Green
    Write-Host "  Telegram:" -ForegroundColor White
    Write-Host "    Enabled: $($status.telegram.enabled)" -ForegroundColor Gray
    Write-Host "    Configured: $($status.telegram.configured)" -ForegroundColor Gray
    Write-Host "    Chat Count: $($status.telegram.chatCount)" -ForegroundColor Gray
    
    Write-Host "  Discord:" -ForegroundColor White
    Write-Host "    Enabled: $($status.discord.enabled)" -ForegroundColor Gray
    Write-Host "    Configured: $($status.discord.configured)" -ForegroundColor Gray
    Write-Host "    Webhook Count: $($status.discord.webhookCount)" -ForegroundColor Gray
    
    Write-Host "  WhatsApp:" -ForegroundColor White
    Write-Host "    Enabled: $($status.whatsapp.enabled)" -ForegroundColor Gray
    Write-Host "    Configured: $($status.whatsapp.configured)" -ForegroundColor Gray
    Write-Host "    Recipient Count: $($status.whatsapp.recipientCount)" -ForegroundColor Gray
    
    $anyEnabled = $status.telegram.enabled -or $status.discord.enabled -or $status.whatsapp.enabled
    $anyConfigured = $status.telegram.configured -or $status.discord.configured -or $status.whatsapp.configured
    
    if ($anyEnabled -and $anyConfigured) {
        Write-Host "`n✅ At least one notification service is enabled and configured" -ForegroundColor Green
    } elseif ($anyEnabled -and -not $anyConfigured) {
        Write-Host "`n⚠️  Services are enabled but not properly configured" -ForegroundColor Yellow
        Write-Host "   Please check your environment variables in docker-compose.yml" -ForegroundColor Yellow
        exit 1
    } else {
        Write-Host "`n⚠️  No notification services are enabled" -ForegroundColor Yellow
        Write-Host "   To enable notifications, edit docker-compose.yml and restart the containers" -ForegroundColor Yellow
        exit 1
    }
    
} catch {
    Write-Host "`n❌ Failed to connect to server" -ForegroundColor Red
    Write-Host "   Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nPlease ensure:" -ForegroundColor Yellow
    Write-Host "  1. Docker containers are running: docker ps" -ForegroundColor White
    Write-Host "  2. Server URL is correct: $ServerUrl" -ForegroundColor White
    exit 1
}

# Test 2: Send test notification
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White
Write-Host "Test 2: Sending Test Notification" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White

$confirm = Read-Host "`nSend test notification? This will trigger alerts on all configured channels (y/n)"

if ($confirm -eq "y") {
    try {
        $testUrl = "$ServerUrl/api/notifications/test"
        $body = @{
            channel = "telegram"
            deviceId = "TEST-DEVICE-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
        } | ConvertTo-Json
        
        Write-Host "Sending test notification..." -ForegroundColor Gray
        
        $response = Invoke-RestMethod -Uri $testUrl -Method Post -Body $body -ContentType "application/json"
        
        Write-Host "`n✅ Test notification sent successfully!" -ForegroundColor Green
        Write-Host "`nResponse:" -ForegroundColor White
        $response | ConvertTo-Json -Depth 10 | Write-Host -ForegroundColor Gray
        
        Write-Host "`nPlease check your configured notification channels:" -ForegroundColor Yellow
        if ($status.telegram.enabled -and $status.telegram.configured) {
            Write-Host "  📱 Telegram: Check your chat/group for the alert" -ForegroundColor White
        }
        if ($status.discord.enabled -and $status.discord.configured) {
            Write-Host "  💬 Discord: Check your channel for the embedded message" -ForegroundColor White
        }
        if ($status.whatsapp.enabled -and $status.whatsapp.configured) {
            Write-Host "  📞 WhatsApp: Check your phone for the message" -ForegroundColor White
        }
        
    } catch {
        Write-Host "`n❌ Failed to send test notification" -ForegroundColor Red
        Write-Host "   Error: $($_.Exception.Message)" -ForegroundColor Red
        
        if ($_.Exception.Response) {
            $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
            $responseBody = $reader.ReadToEnd()
            Write-Host "`n   Server response:" -ForegroundColor Yellow
            Write-Host "   $responseBody" -ForegroundColor Gray
        }
    }
} else {
    Write-Host "`nTest notification skipped." -ForegroundColor Yellow
}

# Test 3: Check Docker logs
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White
Write-Host "Test 3: Recent Docker Logs" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White

$checkLogs = Read-Host "`nView recent Docker logs? (y/n)"

if ($checkLogs -eq "y") {
    Write-Host "`nFetching last 20 lines of API server logs...`n" -ForegroundColor Gray
    try {
        docker logs health-monitor-api --tail 20
    } catch {
        Write-Host "❌ Failed to fetch Docker logs" -ForegroundColor Red
        Write-Host "   Make sure Docker is running and container 'health-monitor-api' exists" -ForegroundColor Yellow
    }
}

# Summary
Write-Host "`n===========================================================" -ForegroundColor Green
Write-Host "  Test Complete" -ForegroundColor Green
Write-Host "===========================================================`n" -ForegroundColor Green

Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  • If test passed: Notifications are working correctly!" -ForegroundColor White
Write-Host "  • If test failed: Check NOTIFICATION_SETUP.md for troubleshooting" -ForegroundColor White
Write-Host "  • View full logs: docker logs health-monitor-api" -ForegroundColor White
Write-Host "  • Monitor alerts: Watch your notification channels for real alerts`n" -ForegroundColor White

Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
