# 通知服務設定指南 / Notification Service Setup Guide

本系統支援多種通知渠道，可在發生以下事件時自動發送警報：
- 🚨 跌倒偵測事件
- 💓 心率異常
- 🌡️ 體溫異常
- 🔊 噪音過高

## 📋 目錄

1. [Telegram 設定](#telegram-設定)
2. [Discord 設定](#discord-設定)
3. [WhatsApp 設定](#whatsapp-設定-via-twilio)
4. [Docker Compose 配置](#docker-compose-配置)
5. [測試通知](#測試通知)

---

## 1️⃣ Telegram 設定

### 步驟 1: 建立 Telegram Bot

1. 在 Telegram 搜尋 `@BotFather`
2. 發送 `/newbot` 指令
3. 按照指示設定 bot 名稱和用戶名
4. 複製 **Bot Token**（格式：`123456789:ABCdefGHIjklMNOpqrsTUVwxyz`）

### 步驟 2: 取得 Chat ID

#### 方法 A: 個人通知
1. 搜尋 `@userinfobot` 並啟動
2. 複製你的 **Chat ID**（數字）

#### 方法 B: 群組通知
1. 建立 Telegram 群組
2. 將你的 bot 加入群組
3. 搜尋 `@RawDataBot` 並加入群組
4. 在群組中發送任何訊息
5. `@RawDataBot` 會回覆群組資訊，找到 `"id"` 欄位（負數）
6. 移除 `@RawDataBot`

### 步驟 3: 配置環境變數

在 `docker-compose.yml` 中設定：

```yaml
TELEGRAM_ENABLED: "true"
TELEGRAM_BOT_TOKEN: "你的_BOT_TOKEN"
TELEGRAM_CHAT_IDS: "123456789,-987654321"  # 多個 ID 用逗號分隔
```

---

## 2️⃣ Discord 設定

### 步驟 1: 建立 Discord Webhook

1. 開啟 Discord 伺服器設定
2. 選擇想要接收通知的頻道
3. 右鍵點擊頻道 → **編輯頻道** → **整合**
4. 點擊 **建立 Webhook**
5. 設定 Webhook 名稱和頭像
6. 點擊 **複製 Webhook URL**
   - URL 格式：`https://discord.com/api/webhooks/123456789/abcdefg...`

### 步驟 2: 配置環境變數

在 `docker-compose.yml` 中設定：

```yaml
DISCORD_ENABLED: "true"
DISCORD_WEBHOOK_URLS: "https://discord.com/api/webhooks/你的webhook_url"
# 多個 webhook 用逗號分隔
```

---

## 3️⃣ WhatsApp 設定 (via Twilio)

### 步驟 1: 註冊 Twilio 帳號

1. 前往 [Twilio 官網](https://www.twilio.com/)
2. 註冊免費試用帳號
3. 完成手機號碼驗證

### 步驟 2: 啟用 WhatsApp Sandbox

1. 登入 Twilio Console
2. 前往 **Messaging** → **Try it out** → **Send a WhatsApp message**
3. 按照指示：
   - 用 WhatsApp 掃描 QR Code 或
   - 發送指定訊息到 Twilio 的 WhatsApp 號碼
4. 完成驗證後，你的 WhatsApp 號碼會連結到 Sandbox

### 步驟 3: 取得憑證

從 Twilio Console 主頁複製：
- **Account SID**（格式：`ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`）
- **Auth Token**（格式：`xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`）

### 步驟 4: 取得 WhatsApp Sandbox 號碼

在 WhatsApp Sandbox 頁面找到：
- **From** 號碼（格式：`+14155238886`）

### 步驟 5: 配置環境變數

在 `docker-compose.yml` 中設定：

```yaml
WHATSAPP_ENABLED: "true"
TWILIO_ACCOUNT_SID: "你的_ACCOUNT_SID"
TWILIO_AUTH_TOKEN: "你的_AUTH_TOKEN"
TWILIO_WHATSAPP_FROM: "+14155238886"  # Twilio Sandbox 號碼
WHATSAPP_TO_NUMBERS: "+886912345678,+886987654321"  # 接收通知的號碼（多個用逗號分隔）
```

### 注意事項
- ⚠️ **Sandbox 限制**：免費試用帳號只能發送給已驗證的號碼
- 💰 **正式使用**：需要升級為付費帳號並申請正式的 WhatsApp Business API
- 📱 **號碼格式**：必須包含國碼（如：+886 為台灣）

---

## 4️⃣ Docker Compose 配置

### 完整配置範例

編輯 `backend/docker-compose.yml`：

```yaml
api:
  environment:
    # ... 其他設定 ...
    
    # Alert Thresholds
    HEART_RATE_MIN: 50
    HEART_RATE_MAX: 120
    BODY_TEMP_MIN: 36.0
    BODY_TEMP_MAX: 38.0
    NOISE_THRESHOLD: 200
    
    # Telegram
    TELEGRAM_ENABLED: "true"
    TELEGRAM_BOT_TOKEN: "123456789:ABCdefGHIjklMNOpqrsTUVwxyz"
    TELEGRAM_CHAT_IDS: "123456789,-987654321"
    
    # Discord
    DISCORD_ENABLED: "true"
    DISCORD_WEBHOOK_URLS: "https://discord.com/api/webhooks/123/abc,https://discord.com/api/webhooks/456/def"
    
    # WhatsApp
    WHATSAPP_ENABLED: "true"
    TWILIO_ACCOUNT_SID: "ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    TWILIO_AUTH_TOKEN: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    TWILIO_WHATSAPP_FROM: "+14155238886"
    WHATSAPP_TO_NUMBERS: "+886912345678,+886987654321"
```

### 重啟服務

```bash
cd backend
docker-compose down
docker-compose up -d --build
```

---

## 5️⃣ 測試通知

### 查看通知服務狀態

```bash
curl http://192.168.1.137:5000/api/notifications/status
```

回應範例：
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
    "enabled": true,
    "configured": true,
    "recipientCount": 2
  }
}
```

### 發送測試通知

使用 PowerShell：

```powershell
$body = @{
    channel = "telegram"
    deviceId = "TEST-DEVICE"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://192.168.1.137:5000/api/notifications/test" `
    -Method Post `
    -Body $body `
    -ContentType "application/json"
```

或使用 curl：

```bash
curl -X POST http://192.168.1.137:5000/api/notifications/test \
  -H "Content-Type: application/json" \
  -d '{"channel":"telegram","deviceId":"TEST-DEVICE"}'
```

---

## 📊 通知訊息格式

### 跌倒警報範例

**Telegram / WhatsApp:**
```
🚨 跌倒警報

⏰ 時間：2025-12-06 14:30:25
📱 裝置：ESP32-001
⚡ 衝擊力：25.50 m/s³
📐 傾斜角度：85.0°
💓 心率：95 bpm
🌡️ 體溫：36.8°C

⚠️ 請立即檢查員工狀況！
```

**Discord:**
- 嵌入式訊息（Embed）
- 紅色標記
- 結構化欄位顯示

---

## ⚙️ 警報閾值調整

在 `docker-compose.yml` 中調整：

```yaml
HEART_RATE_MIN: 50      # 最低心率 (bpm)
HEART_RATE_MAX: 120     # 最高心率 (bpm)
BODY_TEMP_MIN: 36.0     # 最低體溫 (°C)
BODY_TEMP_MAX: 38.0     # 最高體溫 (°C)
NOISE_THRESHOLD: 200    # 噪音閾值 (0-255)
```

---

## 🔧 疑難排解

### 通知未發送

1. **檢查服務狀態**
   ```bash
   curl http://192.168.1.137:5000/api/notifications/status
   ```

2. **檢查 Docker 日誌**
   ```bash
   docker logs health-monitor-api
   ```

3. **驗證環境變數**
   ```bash
   docker exec health-monitor-api env | grep TELEGRAM
   docker exec health-monitor-api env | grep DISCORD
   docker exec health-monitor-api env | grep WHATSAPP
   ```

### Telegram 錯誤

- **401 Unauthorized**: Bot Token 錯誤
- **400 Bad Request**: Chat ID 格式錯誤
- **403 Forbidden**: Bot 被封鎖或未啟動對話

### Discord 錯誤

- **404 Not Found**: Webhook URL 錯誤或已被刪除
- **429 Too Many Requests**: 超過速率限制

### WhatsApp/Twilio 錯誤

- **20003**: 認證失敗（Account SID 或 Auth Token 錯誤）
- **21211**: 無效的電話號碼格式
- **21614**: 號碼未驗證（Sandbox 限制）

---

## 📝 安全建議

1. **不要在程式碼中直接寫入憑證**
2. **使用 `.env` 檔案管理敏感資訊**
3. **確保 `.env` 已加入 `.gitignore`**
4. **定期更換 API 金鑰**
5. **限制 Discord Webhook 權限**
6. **保護 Twilio 憑證不外洩**

---

## 📞 支援

如有問題，請檢查：
1. Docker 容器是否正常運行：`docker ps`
2. API 日誌：`docker logs health-monitor-api`
3. 測試端點：`http://192.168.1.137:5000/api/notifications/status`

---

**建立日期**: 2025-12-06  
**系統版本**: v1.0.0
