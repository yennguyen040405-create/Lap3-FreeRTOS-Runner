Họ tên: Nguyễn Yến
Lớp: Kỹ sư Kỹ thuật Viễn thông – Khóa 64
ESP32 kết hợp cảm biến BMP180 để thu thập dữ liệu nhiệt độ, áp suất và độ cao. Hệ thống sử dụng FreeRTOS để quản lý các nhiệm vụ đọc dữ liệu, xử lý cảnh báo và truyền thông MQTT. Khi phát hiện nhiệt độ hoặc độ cao thay đổi vượt ngưỡng cài đặt, hệ thống tự động điều khiển LED và Buzzer cảnh báo. Dữ liệu được gửi lên MQTT Broker (broker.emqx.io) và hiển thị trên Website, hỗ trợ giám sát và điều khiển từ xa.
Task	Core	Nhiệm vụ
MQTT_Task	0	Kết nối Wi-Fi/MQTT, gửi dữ liệu và nhận lệnh từ Web
Sensor_Task	1	Đọc BMP180, thu thập nhiệt độ, áp suất và độ cao
Alert_Task	1	Xử lý cảnh báo, điều khiển LED và Buzzer
Các Task trao đổi dữ liệu thông qua Queue và sử dụng Mutex để bảo vệ dữ liệu dùng chung.
- BMP180 SDA: GPIO21, SCL: GPIO22.
- LED cảnh báo: GPIO18.
- Buzzer: GPIO19.
- Nhiệt độ cao: lớn hơn 35°C.
- Nhiệt độ thấp: nhỏ hơn 10°C.
- Độ cao thay đổi: chênh lệch lớn hơn 3 m so với lần đo trước.
- Cảnh báo thay đổi độ cao được duy trì trong 3 giây.
- Ở chế độ AUTO, LED và Buzzer nhấp nháy khi phát hiện điều kiện bất thường.
- MQTT Broker: broker.emqx.io
- Port: 1883
- WebSocket Secure: wss://broker.emqx.io:8084/mqtt
Các Topic sử dụng:
- lab3/runner/nguyenyen_dhtd22/data/data: Gửi dữ liệu cảm biến và trạng thái thiết bị.
- lab3/runner/nguyenyen_dhtd22/data/cmd/led: Điều khiển LED.
- lab3/runner/nguyenyen_dhtd22/data/cmd/buzzer: Điều khiển Buzzer.
- lab3/runner/nguyenyen_dhtd22/data/cmd/mode: Chuyển chế độ AUTO/MANUAL.
- Arduino IDE: Lập trình ESP32.
- Wokwi: Mô phỏng hệ thống.
- MQTTX: Kiểm tra kết nối, Subscribe và Publish dữ liệu MQTT.
- MQTT.js: Kết nối Website với MQTT Broker.
- HTML, CSS, JavaScript: Xây dựng giao diện Website.
- Runner.ino: Code ESP32.
- index.html: Giao diện Website.
- style.css: Thiết kế giao diện.
- script.js: Xử lý kết nối và trao đổi dữ liệu MQTT.
- BaoCao_FreeRTOS_Runner.docx: Báo cáo.
- Images/: Ảnh mô hình và kết quả thực nghiệm.
- Đọc và hiển thị dữ liệu nhiệt độ, áp suất, độ cao.
- Gửi dữ liệu từ ESP32 lên MQTT Broker.
- Hiển thị dữ liệu trên Website.
- Tự động cảnh báo bằng LED và Buzzer.
- Điều khiển LED, Buzzer thông qua Website.
- Giám sát và kiểm tra dữ liệu bằng MQTTX.
https://wokwi.com/projects/479989739624807425
https://mqttx.app/web-client#/recent_connections/eb5c84d7-d628-4512-9871-c3b2502634f2
