# Docker 部署命令參考

## 快速啟動

### 啟動所有服務
```powershell
cd C:\Users\user\Code\Project\IBSP\backend
docker-compose up -d
```

### 重新構建並啟動（在代碼更新後）
```powershell
docker-compose down
docker-compose up -d --build
```

## 服務管理

### 查看容器狀態
```powershell
docker ps -a --filter "name=health-monitor"
```

### 查看日誌
```powershell
# API 服務器日誌
docker logs health-monitor-api --tail 50 -f

# Dashboard 日誌
docker logs health-monitor-dashboard --tail 50 -f

# 資料庫日誌
docker logs health-monitor-db --tail 50 -f
```

### 停止服務
```powershell
docker-compose down
```

### 停止並刪除所有數據（包括資料庫）
```powershell
docker-compose down -v
```

### 重啟特定服務
```powershell
# 重啟 API
docker-compose restart api

# 重啟 Dashboard
docker-compose restart dashboard

# 重啟資料庫
docker-compose restart postgres
```

## 服務訪問

### 端口映射
- **API Server**: http://localhost:5000 (容器內部端口: 3000)
- **Dashboard**: http://localhost:5001 (容器內部端口: 3001)
- **PostgreSQL**: localhost:5432 (容器內部端口: 5432)

### API 端點測試
```powershell
# 健康檢查
Invoke-WebRequest -Uri http://localhost:5000/health -Method GET | Select-Object -ExpandProperty Content

# 獲取設備列表
Invoke-WebRequest -Uri http://localhost:5000/api/devices -Method GET | Select-Object -ExpandProperty Content

# 獲取追蹤狀態
Invoke-WebRequest -Uri http://localhost:5000/api/tracking/ESP32-001 -Method GET | Select-Object -ExpandProperty Content

# 重置追蹤點
Invoke-WebRequest -Uri http://localhost:5000/api/tracking/reset/ESP32-001 -Method POST | Select-Object -ExpandProperty Content
```

## 調試

### 進入容器內部
```powershell
# 進入 API 容器
docker exec -it health-monitor-api sh

# 進入資料庫容器
docker exec -it health-monitor-db psql -U healthmonitor -d health_monitoring
```

### 查看容器資源使用情況
```powershell
docker stats health-monitor-api health-monitor-dashboard health-monitor-db
```

### 查看網絡連接
```powershell
docker network inspect backend_health-network
```

## 更新代碼流程

1. 修改代碼（如 server.js）
2. 停止容器：`docker-compose down`
3. 重新構建並啟動：`docker-compose up -d --build`
4. 檢查日誌：`docker logs health-monitor-api -f`

## 當前服務狀態

✅ **health-monitor-db**: 運行中 (健康)
✅ **health-monitor-api**: 運行中，包含新的追蹤功能
✅ **health-monitor-dashboard**: 運行中

## 新功能端點

### 追蹤管理
- `POST /api/tracking/reset/:device_id` - 重置追蹤起始時間
- `GET /api/tracking/:device_id` - 獲取追蹤資訊
- `DELETE /api/tracking/reset/:device_id` - 清除追蹤過濾器

### 資料查詢（自動過濾追蹤時間點）
- `GET /api/realtime/:device_id` - 即時數據
- `GET /api/ecg/:device_id` - 心電圖數據
- `GET /api/falls/:device_id` - 跌倒事件
- `GET /api/falls/alerts/pending` - 待處理警報

## 備份與恢復

### 備份資料庫
```powershell
docker exec health-monitor-db pg_dump -U healthmonitor health_monitoring > backup.sql
```

### 恢復資料庫
```powershell
Get-Content backup.sql | docker exec -i health-monitor-db psql -U healthmonitor -d health_monitoring
```
