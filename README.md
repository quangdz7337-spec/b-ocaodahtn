

## 1. Tổng quan hệ thống

AI nhận diện giọng nói (I2S microphone + Edge Impulse)

Cảm biến phát hiện người (RADAR/PIR)

DHT22 đo nhiệt độ / độ ẩm

LDR đo ánh sáng môi trường

ACS712 đo dòng điện (kiểm tra quá tải)

WiFi + API server (Flask/NodeJS)

LCD I2C hiển thị trạng thái

Relay điều khiển đèn + quạt

LED cảnh báo quá dòng


---

## 🔄 2. Nguyên lý hoạt động

### 🚀 Khi khởi động
- Khởi tạo cảm biến + LCD + AI microphone
- Relay OFF, hệ thống chờ radar

---

### 👤 Khi phát hiện người

1. Radar = ON → kích hoạt hệ thống
2. Đọc LDR:
   - Tối → bật đèn
   - Sáng → tắt đèn
3. Kết nối WiFi
4. Chuyển sang chế độ SYSTEM READY

---

### 🧠 AI Voice Control (Edge Impulse)

ESP32 liên tục thu âm qua I2S:

| Lệnh AI | Hành động |
|--------|----------|
| Batden | Bật đèn |
| Tatden | Tắt đèn |
| Batquat | Bật quạt |
| Tatquat | Tắt quạt |

---

### 🌡️ Cảm biến môi trường (1s)

- DHT22 → nhiệt độ + độ ẩm
- ACS712 → dòng điện đèn/quạt

---

### ⚠️ Bảo vệ quá dòng

Nếu dòng vượt ngưỡng:
- Tắt toàn bộ relay
- Bật LED cảnh báo
- LCD hiển thị “OVER CURRENT”

---

### 🌐 IoT Server (5s)

ESP32 gửi dữ liệu:

{
  "temperature": 30,
  "humidity": 60,
  "currentL": 0.2,
  "currentF": 0.1,
  "presence": true,
  "light": true,
  "fan": false
}
