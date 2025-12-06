# Health Monitoring System - Notification Services Quick Setup
# ===========================================================

Write-Host "`n===========================================================" -ForegroundColor Cyan
Write-Host "  Health Monitoring System - Notification Setup" -ForegroundColor Cyan
Write-Host "===========================================================`n" -ForegroundColor Cyan

Write-Host "This script will help you configure notification services." -ForegroundColor Yellow
Write-Host "You can enable Telegram, Discord, and/or WhatsApp notifications.`n" -ForegroundColor Yellow

# Read current docker-compose.yml
$dockerComposePath = Join-Path $PSScriptRoot "docker-compose.yml"

if (-not (Test-Path $dockerComposePath)) {
    Write-Host "Error: docker-compose.yml not found in current directory!" -ForegroundColor Red
    Write-Host "Please run this script from the backend directory." -ForegroundColor Red
    exit 1
}

Write-Host "Found docker-compose.yml: $dockerComposePath`n" -ForegroundColor Green

# Function to update environment variable in docker-compose.yml
function Update-DockerComposeEnv {
    param(
        [string]$Key,
        [string]$Value
    )
    
    $content = Get-Content $dockerComposePath -Raw
    $pattern = "(\s+$Key\s*:\s*)`"[^`"]*`""
    $replacement = "`${1}`"$Value`""
    $content = $content -replace $pattern, $replacement
    Set-Content -Path $dockerComposePath -Value $content -NoNewline
}

# Ask about Telegram
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "  TELEGRAM CONFIGURATION" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

$enableTelegram = Read-Host "Enable Telegram notifications? (y/n)"
if ($enableTelegram -eq "y") {
    $botToken = Read-Host "Enter Telegram Bot Token (from @BotFather)"
    $chatIds = Read-Host "Enter Chat IDs (comma-separated, e.g., 123456789,-987654321)"
    
    Update-DockerComposeEnv "TELEGRAM_ENABLED" "true"
    Update-DockerComposeEnv "TELEGRAM_BOT_TOKEN" $botToken
    Update-DockerComposeEnv "TELEGRAM_CHAT_IDS" $chatIds
    
    Write-Host "✅ Telegram configured successfully!`n" -ForegroundColor Green
} else {
    Update-DockerComposeEnv "TELEGRAM_ENABLED" "false"
    Write-Host "⚠️  Telegram notifications disabled.`n" -ForegroundColor Yellow
}

# Ask about Discord
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "  DISCORD CONFIGURATION" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

$enableDiscord = Read-Host "Enable Discord notifications? (y/n)"
if ($enableDiscord -eq "y") {
    $webhookUrls = Read-Host "Enter Discord Webhook URLs (comma-separated)"
    
    Update-DockerComposeEnv "DISCORD_ENABLED" "true"
    Update-DockerComposeEnv "DISCORD_WEBHOOK_URLS" $webhookUrls
    
    Write-Host "✅ Discord configured successfully!`n" -ForegroundColor Green
} else {
    Update-DockerComposeEnv "DISCORD_ENABLED" "false"
    Write-Host "⚠️  Discord notifications disabled.`n" -ForegroundColor Yellow
}

# Ask about WhatsApp
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "  WHATSAPP CONFIGURATION (via Twilio)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

$enableWhatsApp = Read-Host "Enable WhatsApp notifications? (y/n)"
if ($enableWhatsApp -eq "y") {
    $accountSid = Read-Host "Enter Twilio Account SID"
    $authToken = Read-Host "Enter Twilio Auth Token" -AsSecureString
    $authTokenPlain = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($authToken))
    $fromNumber = Read-Host "Enter Twilio WhatsApp From Number (e.g., +14155238886)"
    $toNumbers = Read-Host "Enter recipient phone numbers (comma-separated, e.g., +886912345678)"
    
    Update-DockerComposeEnv "WHATSAPP_ENABLED" "true"
    Update-DockerComposeEnv "TWILIO_ACCOUNT_SID" $accountSid
    Update-DockerComposeEnv "TWILIO_AUTH_TOKEN" $authTokenPlain
    Update-DockerComposeEnv "TWILIO_WHATSAPP_FROM" $fromNumber
    Update-DockerComposeEnv "WHATSAPP_TO_NUMBERS" $toNumbers
    
    Write-Host "✅ WhatsApp configured successfully!`n" -ForegroundColor Green
} else {
    Update-DockerComposeEnv "WHATSAPP_ENABLED" "false"
    Write-Host "⚠️  WhatsApp notifications disabled.`n" -ForegroundColor Yellow
}

# Summary
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Green
Write-Host "  CONFIGURATION COMPLETE" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Green

Write-Host "Configuration has been saved to docker-compose.yml" -ForegroundColor Green
Write-Host "`nNext steps:" -ForegroundColor Yellow
Write-Host "  1. Review docker-compose.yml to verify settings" -ForegroundColor White
Write-Host "  2. Restart Docker containers:" -ForegroundColor White
Write-Host "     docker-compose down" -ForegroundColor Cyan
Write-Host "     docker-compose up -d --build" -ForegroundColor Cyan
Write-Host "  3. Check service status:" -ForegroundColor White
Write-Host "     curl http://192.168.1.137:5000/api/notifications/status" -ForegroundColor Cyan
Write-Host "  4. Send test notification:" -ForegroundColor White
Write-Host "     curl -X POST http://192.168.1.137:5000/api/notifications/test" -ForegroundColor Cyan
Write-Host "`nFor detailed setup instructions, see NOTIFICATION_SETUP.md`n" -ForegroundColor Yellow

Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
