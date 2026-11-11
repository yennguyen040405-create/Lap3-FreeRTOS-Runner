
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <stdarg.h>
#include <math.h>

// ==================================================
// 1. CAU HINH WIFI VA MQTT
// ==================================================

#define WIFI_SSID   "Wokwi-GUEST"
#define WIFI_PASS   ""
#define WIFI_CH     6

#define MQTT_HOST   "broker.emqx.io"
#define MQTT_PORT   1883

#define TOPIC_BASE  "lab3/runner/nguyenyen_dhtd22/data"

#define T_DATA      TOPIC_BASE "/data"
#define T_CMD_LED   TOPIC_BASE "/cmd/led"
#define T_CMD_BUZ   TOPIC_BASE "/cmd/buzzer"
#define T_CMD_MODE  TOPIC_BASE "/cmd/mode"

// ==================================================
// 2. CAU HINH CHAN
// ==================================================

#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_LED     18
#define PIN_BUZZER  19

// ==================================================
// 3. NGUONG CANH BAO
// ==================================================

#define TEMP_HIGH   35.0f
#define TEMP_LOW    10.0f
#define ALT_JUMP    3.0f
#define SEA_LEVEL   101325

// ==================================================
// 4. CAU TRUC DU LIEU
// ==================================================

struct SensorData {
    float temp;
    float pressure;
    float altitude;
};

struct AlertData {
    float temp;
    float altitude;
};

struct DeviceState {
    bool manual;
    bool cmdLed;
    bool cmdBuzzer;
    bool led;
    bool buzzer;
    char alert[16];
};

// ==================================================
// 5. KHAI BAO QUEUE VA MUTEX
// ==================================================

QueueHandle_t dataQueue;
QueueHandle_t alertQueue;

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t stateMutex;

DeviceState state = {
    false, false, false, false, false, "NORMAL"
};

// ==================================================
// 6. KHOI TAO THIET BI
// ==================================================

Adafruit_BMP085 bmp;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ==================================================
// 7. HAM IN SERIAL CO MUTEX
// ==================================================

void logLine(const char *fmt, ...) {

    char buf[180];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (serialMutex != NULL &&
        xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        Serial.println(buf);

        xSemaphoreGive(serialMutex);
    }
}

// ==================================================
// 8. NHAN LENH MQTT TU WEBSITE
// ==================================================

void onMqttMessage(char *topic, byte *payload, unsigned int len) {

    String msg;

    for (unsigned int i = 0; i < len; i++) {
        msg += (char)payload[i];
    }

    msg.trim();
    msg.toUpperCase();

    bool on = (msg == "ON" || msg == "1");

    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        if (strcmp(topic, T_CMD_LED) == 0) {
            state.cmdLed = on;
        }

        else if (strcmp(topic, T_CMD_BUZ) == 0) {
            state.cmdBuzzer = on;
        }

        else if (strcmp(topic, T_CMD_MODE) == 0) {

            if (msg == "MANUAL") {
                state.manual = true;
            }

            else if (msg == "AUTO") {
                state.manual = false;
            }
        }

        xSemaphoreGive(stateMutex);
    }

    logLine("[MQTT] %s = %s", topic, msg.c_str());
}

// ==================================================
// 9. KET NOI LAI MQTT
// ==================================================

bool mqttReconnect() {

    String cid = "esp32-runner-" +
                 String((uint32_t)ESP.getEfuseMac(), HEX);

    logLine("[MQTT] Dang ket noi broker...");

    if (mqtt.connect(cid.c_str())) {

        mqtt.subscribe(T_CMD_LED);
        mqtt.subscribe(T_CMD_BUZ);
        mqtt.subscribe(T_CMD_MODE);

        logLine("[MQTT] Da ket noi broker");
        logLine("[MQTT] Da subscribe 3 topic dieu khien");

        return true;
    }

    logLine("[MQTT] Ket noi that bai, rc=%d", mqtt.state());

    return false;
}

// ==================================================
// 10. TASK MQTT - CORE 0
// ==================================================

void taskMQTT(void *pv) {

    // Ket noi WiFi
    logLine("[WiFi] Dang ket noi %s ...", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CH);

    while (WiFi.status() != WL_CONNECTED) {

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    logLine("[WiFi] Thanh cong, IP: %s",
            WiFi.localIP().toString().c_str());

    // Cau hinh MQTT
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setBufferSize(512);
    mqtt.setSocketTimeout(2);
    mqtt.setKeepAlive(30);

    SensorData d;
    DeviceState s;

    char json[320];

    for (;;) {

        // Kiem tra WiFi
        if (WiFi.status() != WL_CONNECTED) {

            logLine("[WiFi] Mat ket noi, dang ket noi lai...");

            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CH);

            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // Kiem tra MQTT
        if (!mqtt.connected()) {

            if (!mqttReconnect()) {

                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        // Xu ly MQTT va nhan lenh dieu khien
        mqtt.loop();

        // Nhan du lieu tu dataQueue
        if (xQueueReceive(
                dataQueue,
                &d,
                pdMS_TO_TICKS(100)
            ) == pdTRUE) {

            if (xSemaphoreTake(
                    stateMutex,
                    pdMS_TO_TICKS(50)
                ) == pdTRUE) {

                s = state;

                xSemaphoreGive(stateMutex);

                // Tao JSON
                snprintf(json, sizeof(json),
                    "{\"temp\":%.2f,\"press\":%.2f,"
                    "\"alt\":%.2f,\"led\":%d,"
                    "\"buzzer\":%d,\"mode\":\"%s\","
                    "\"alert\":\"%s\"}",

                    d.temp,
                    d.pressure,
                    d.altitude,
                    s.led,
                    s.buzzer,
                    s.manual ? "MANUAL" : "AUTO",
                    s.alert
                );

                // Gui du lieu len MQTT Broker
                if (!mqtt.publish(T_DATA, json)) {
                    logLine("[MQTT] Gui telemetry that bai");
                }
            }
        }

        // Nhuong CPU cho cac Task khac
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ==================================================
// 11. TASK DOC CAM BIEN BMP180 - CORE 1
// ==================================================

void taskSensor(void *pv) {

    Wire.begin(PIN_SDA, PIN_SCL);

    // Kiem tra BMP180
    while (!bmp.begin()) {

        logLine("[BMP180] Khong tim thay cam bien!");
        logLine("[BMP180] Kiem tra day SDA va SCL");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    logLine("[BMP180] San sang");

    TickType_t last = xTaskGetTickCount();

    for (;;) {

        SensorData d;

        // Doc nhiet do
        d.temp = bmp.readTemperature();

        // Doc ap suat
        d.pressure = bmp.readPressure() / 100.0f;

        // Doc do cao
        d.altitude = bmp.readAltitude(SEA_LEVEL);

        // Hien thi Serial
        logLine(
            "T = %.2f C | P = %.2f hPa | Alt = %.2f m",
            d.temp,
            d.pressure,
            d.altitude
        );

        // Gui du lieu vao dataQueue
        if (xQueueSend(dataQueue, &d, 0) != pdPASS) {
            logLine("[QUEUE] dataQueue day");
        }

        // Gui nhiet do va do cao vao alertQueue
        AlertData a = {
            d.temp,
            d.altitude
        };

        if (xQueueSend(alertQueue, &a, 0) != pdPASS) {
            logLine("[QUEUE] alertQueue day");
        }

        // Chu ky doc cam bien 1 giay
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    }
}

// ==================================================
// 12. TASK CANH BAO - CORE 1
// ==================================================

void taskAlert(void *pv) {

    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);

    AlertData a;

    float prevAlt = NAN;

    unsigned long jumpUntil = 0;

    char alertMsg[16] = "NORMAL";

    bool lastBuz = false;

    for (;;) {

        // Nhan du lieu tu alertQueue
        if (xQueueReceive(
                alertQueue,
                &a,
                pdMS_TO_TICKS(100)
            ) == pdTRUE) {

            strcpy(alertMsg, "NORMAL");

            // Kiem tra nhiet do
            if (a.temp > TEMP_HIGH) {
                strcpy(alertMsg, "TEMP_HIGH");
            }

            else if (a.temp < TEMP_LOW) {
                strcpy(alertMsg, "TEMP_LOW");
            }

            // Kiem tra thay doi do cao dot ngot
            if (!isnan(prevAlt) &&
                fabsf(a.altitude - prevAlt) > ALT_JUMP) {

                strcpy(alertMsg, "ALT_JUMP");

                jumpUntil = millis() + 3000;
            }

            else if (millis() < jumpUntil) {

                strcpy(alertMsg, "ALT_JUMP");
            }

            prevAlt = a.altitude;

            if (strcmp(alertMsg, "NORMAL") != 0) {
                logLine("[ALERT] %s", alertMsg);
            }
        }

        // Doc trang thai dieu khien
        DeviceState s;

        if (xSemaphoreTake(
                stateMutex,
                pdMS_TO_TICKS(20)
            ) != pdTRUE) {

            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        s = state;

        xSemaphoreGive(stateMutex);

        bool ledOut;
        bool buzOut;

        // Tao hieu ung nhap nhay
        bool blink = ((millis() / 250) % 2) == 0;

        // Che do thu cong
        if (s.manual) {

            ledOut = s.cmdLed;
            buzOut = s.cmdBuzzer;
        }

        // Che do tu dong
        else {

            bool alerting =
                strcmp(alertMsg, "NORMAL") != 0;

            ledOut = alerting && blink;
            buzOut = alerting && blink;
        }

        // Dieu khien LED
        digitalWrite(PIN_LED, ledOut ? HIGH : LOW);

        // Dieu khien Buzzer
        if (buzOut != lastBuz) {

            if (buzOut) {
                tone(PIN_BUZZER, 1000);
            }

            else {
                noTone(PIN_BUZZER);
            }

            lastBuz = buzOut;
        }

        // Cap nhat trang thai thiet bi
        if (xSemaphoreTake(
                stateMutex,
                pdMS_TO_TICKS(20)
            ) == pdTRUE) {

            state.led = ledOut;
            state.buzzer = buzOut;

            strcpy(state.alert, alertMsg);

            xSemaphoreGive(stateMutex);
        }

        // Nhuong CPU cho Task khac
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ==================================================
// 13. SETUP
// ==================================================

void setup() {

    Serial.begin(115200);

    // Tao Mutex
    serialMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();

    // Tao Queue
    dataQueue = xQueueCreate(10, sizeof(SensorData));
    alertQueue = xQueueCreate(5, sizeof(AlertData));

    // Kiem tra khoi tao
    if (serialMutex == NULL ||
        stateMutex == NULL ||
        dataQueue == NULL ||
        alertQueue == NULL) {

        Serial.println("[ERROR] Khoi tao Queue/Mutex that bai!");
        while (true) {
            delay(1000);
        }
    }

    // Tao Task MQTT - Core 0
    BaseType_t t1 = xTaskCreatePinnedToCore(
        taskMQTT,
        "MQTT_Task",
        8192,
        NULL,
        1,
        NULL,
        0
    );

    // Tao Task Sensor - Core 1
    BaseType_t t2 = xTaskCreatePinnedToCore(
        taskSensor,
        "Sensor_Task",
        4096,
        NULL,
        2,
        NULL,
        1
    );

    // Tao Task Alert - Core 1
    BaseType_t t3 = xTaskCreatePinnedToCore(
        taskAlert,
        "Alert_Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    if (t1 != pdPASS || t2 != pdPASS || t3 != pdPASS) {

        Serial.println("[ERROR] Tao Task that bai!");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("[SYSTEM] Khoi tao FreeRTOS thanh cong");
}

// ==================================================
// 14. LOOP
// ==================================================

void loop() {

    vTaskDelay(portMAX_DELAY);
}
