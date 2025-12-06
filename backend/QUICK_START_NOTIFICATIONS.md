# 通知服務快速開始指南 / Quick Start Guide for Notifications

## 🚀 最快設定方式 / Fastest Setup

### 1. 選擇通知平台 / Choose Your Platform

推薦新手使用 **Telegram**（最簡單）或 **Discord**（免費且功能強大）

### 2. Telegram 快速設定（5分鐘）

#### Step 1: 建立 Bot
1. 在 Telegram 搜尋 `@BotFather`
2. 發送 `/newbot`
3. 設定名稱，例如：`Health Monitor Alert Bot`
4. 設定用戶名，例如：`health_alert_bot`
5. **複製 Bot Token**（保存好！）

#### Step 2: 取得 Chat ID
1. 在 Telegram 搜尋 `@userinfobot`
2. 點擊 Start
3. **複製你的 Chat ID**（數字）

#### Step 3: 配置系統
編輯 `backend/docker-compose.yml`，找到 `api` 服務的 environment 區塊：

```yaml
TELEGRAM_ENABLED: "true"
TELEGRAM_BOT_TOKEN: "你的_BOT_TOKEN"
TELEGRAM_CHAT_IDS: "你的_CHAT_ID"
```

#### Step 4: 重啟系統
```bash
cd backend
docker-compose down
docker-compose up -d --build
```

#### Step 5: 測試
```powershell
.\test-notifications.ps1
```

✅ 完成！你應該會在 Telegram 收到測試訊息

---

### 3. Discord 快速設定（3分鐘）

#### Step 1: 建立 Webhook
1. 開啟 Discord，選擇伺服器和頻道
2. 右鍵點擊頻道 → **編輯頻道**
3. 選擇 **整合** → **建立 Webhook**
4. 設定名稱：`Health Monitor`
5. **複製 Webhook URL**

#### Step 2: 配置系統
編輯 `backend/docker-compose.yml`：

```yaml
DISCORD_ENABLED: "true"
DISCORD_WEBHOOK_URLS: "你的_WEBHOOK_URL"
```

#### Step 3: 重啟並測試
```bash
cd backend
docker-compose down
docker-compose up -d --build
.\test-notifications.ps1
```

✅ 完成！在 Discord 頻道查看測試訊息

---

### 4. WhatsApp 設定（需要 Twilio 帳號）

⚠️ **注意**：Twilio 免費試用有限制，僅能發送給已驗證的號碼

#### Step 1: 註冊 Twilio
1. 前往 https://www.twilio.com/
2. 註冊免費試用帳號
3. 完成手機驗證

#### Step 2: 啟用 WhatsApp Sandbox
1. 在 Twilio Console，前往 **Messaging** → **Try it out** → **Send a WhatsApp message**
2. 掃描 QR Code 或發送訊息到 Twilio 的 WhatsApp 號碼
3. 複製以下資訊：
   - Account SID
   - Auth Token
   - WhatsApp From 號碼（例如：`+14155238886`）

#### Step 3: 配置系統
編輯 `backend/docker-compose.yml`：

```yaml
WHATSAPP_ENABLED: "true"
TWILIO_ACCOUNT_SID: "你的_ACCOUNT_SID"
TWILIO_AUTH_TOKEN: "你的_AUTH_TOKEN"
TWILIO_WHATSAPP_FROM: "+14155238886"
WHATSAPP_TO_NUMBERS: "+886912345678"  # 你的手機號碼（含國碼）
```

#### Step 4: 重啟並測試
```bash
cd backend
docker-compose down
docker-compose up -d --build
.\test-notifications.ps1
```

✅ 完成！在 WhatsApp 查看測試訊息

---

## 📊 警報類型

系統會在以下情況自動發送警報：

| 警報類型 | 觸發條件 | 預設閾值 | 冷卻時間 |
|---------|---------|---------|---------|
| 🚨 跌倒 | 偵測到跌倒事件 | N/A | 無（立即發送） |
| 💓 心率異常 | 超出正常範圍 | 50-120 bpm | 5分鐘 |
| 🌡️ 體溫異常 | 超出正常範圍 | 36-38°C | 5分鐘 |
| 🔊 噪音過高 | 超過閾值 | 200/255 | 5分鐘 |

## 🔧 調整警報閾值

在 `docker-compose.yml` 中修改：

```yaml
HEART_RATE_MIN: 50       # 最低心率 (bpm)
HEART_RATE_MAX: 120      # 最高心率 (bpm)
BODY_TEMP_MIN: 36.0      # 最低體溫 (°C)
BODY_TEMP_MAX: 38.0      # 最高體溫 (°C)
NOISE_THRESHOLD: 200     # 噪音閾值 (0-255)
```

重啟後生效：
```bash
docker-compose restart api
```

## 🧪 測試工具

### 查看通知狀態
```powershell
curl http://192.168.1.137:5000/api/notifications/status
```

### 發送測試通知
```powershell
.\test-notifications.ps1
```

### 查看系統日誌
```bash
docker logs health-monitor-api -f
```

## ❓ 常見問題

### Q: 沒有收到通知？
1. 檢查環境變數是否正確設定
2. 執行 `.\test-notifications.ps1` 檢查配置
3. 查看 Docker 日誌：`docker logs health-monitor-api`

### Q: Telegram 顯示 401 錯誤？
- Bot Token 錯誤，請重新檢查並複製正確的 Token

### Q: Discord Webhook 404 錯誤？
- Webhook URL 可能被刪除或錯誤，請重新建立

### Q: WhatsApp 無法發送？
- 免費試用只能發送給已驗證的號碼
- 確保號碼格式包含國碼（如：+886）

### Q: 如何同時啟用多個通知渠道？
- 可以同時設定 Telegram、Discord、WhatsApp 為 `"true"`
- 系統會同時發送到所有已配置的渠道

## 📚 詳細文檔

完整設定說明請參閱：[NOTIFICATION_SETUP.md](NOTIFICATION_SETUP.md)

## 🎯 下一步

1. ✅ 配置至少一個通知渠道
2. ✅ 測試通知功能
3. ✅ 調整警報閾值
4. ✅ 部署 ESP32 設備
5. 📊 監控系統運行

---

**需要幫助？** 查看詳細文檔或檢查 Docker 日誌
