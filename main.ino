//======================================================
// SMART ROOM AI SYSTEM
// FreeRTOS Version - Fixed All Bugs
// ESP32 Dual Core
//======================================================

#include <quang1288-project-1_inferencing.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <driver/i2s.h>

#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ACS712.h>

//======================================================
// WIFI
//======================================================
const char* ssid = "quamondi";
const char* pass = "12345678";

//======================================================
// API
//======================================================
const char* API_DATA    = "http://10.103.107.32:5000/api/data";
const char* API_CONTROL = "http://10.103.107.32:5000/api/control";

//======================================================
// I2S PIN
//======================================================
#define I2S_WS    25
#define I2S_SCK   32
#define I2S_SD    33
#define I2S_PORT  I2S_NUM_0

//======================================================
// PIN
//======================================================
#define LED_PIN       14
#define RELAY_LIGHT   26
#define RELAY_FAN     27
#define RADAR_PIN     16
#define LDR_PIN       36
#define DHTPIN         4
#define DHTTYPE       DHT22
#define ACS_LIGHT     35
#define ACS_FAN       34

//======================================================
// THRESHOLD
//======================================================
#define LDR_THRESHOLD      1500
#define CURRENT_THRESHOLD  10.0f

//======================================================
// OBJECTS
//======================================================
DHT               dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
ACS712            acsL(ACS_LIGHT, 3.3, 4095, 185);
ACS712            acsF(ACS_FAN,   3.3, 4095, 185);

//======================================================
// GLOBAL STATE — protected by stateMutex
//======================================================
volatile bool  presence    = false;
volatile bool  lightState  = false;
volatile bool  fanState    = false;
volatile bool  overCurrent = false;
volatile float temperature = 0;
volatile float humidity    = 0;
volatile float currentL    = 0;
volatile float currentF    = 0;

//======================================================
// AI BUFFER
//======================================================
static signed short sampleBuffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT];

//======================================================
// MUTEX & SEMAPHORE
//======================================================
SemaphoreHandle_t stateMutex;
SemaphoreHandle_t lcdMutex;
SemaphoreHandle_t i2sMutex;

//======================================================
// FORWARD DECLARATIONS
//======================================================
void setupI2S();
void connectWiFi();
void readSensor();
void updateLCD();
void sendWeb();
void getWebControl();
void runAIInference();
int  readLDR();

int  ei_get_data(size_t offset, size_t length, float* out_ptr);

//======================================================
// WIFI — blocking once at startup, has timeout
//======================================================
void connectWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting WiFi");

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED) {

    vTaskDelay(pdMS_TO_TICKS(500));

    Serial.print(".");

    retry++;

    if (retry > 40) {

      Serial.println("\nWiFi timeout, retrying...");

      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(1000));
      WiFi.begin(ssid, pass);

      retry = 0;
    }
  }

  Serial.println();
  Serial.print("WiFi Connected. IP: ");
  Serial.println(WiFi.localIP());
}

//======================================================
// READ SENSOR — writes protected by mutex
//======================================================
void readSensor() {

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t)) t = 0;
  if (isnan(h)) h = 0;

  int curL = acsL.mA_AC(20, 1);
  int curF = acsF.mA_AC(20, 1);

  if (abs(curL) < 200) curL = 0;
  if (abs(curF) < 200) curF = 0;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  temperature = t;
  humidity    = h;
  currentL    = curL / 1000.0f;
  currentF    = curF / 1000.0f;
  xSemaphoreGive(stateMutex);
}

//======================================================
// READ LDR — no shared state, no mutex needed
//======================================================
int readLDR() {

  int sum = 0;

  for (int i = 0; i < 5; i++) {
    sum += analogRead(LDR_PIN);
    vTaskDelay(2);
  }

  return sum / 5;
}

//======================================================
// LCD UPDATE — reads protected by mutex, lcd by lcdMutex
//======================================================
void updateLCD() {

  // Snapshot shared state
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  int   t   = (int)temperature;
  int   h   = (int)humidity;
  float cL  = currentL;
  float cF  = currentF;
  bool  oc  = overCurrent;
  xSemaphoreGive(stateMutex);

  xSemaphoreTake(lcdMutex, portMAX_DELAY);

  if (oc) {

    lcd.setCursor(0, 0);
    lcd.print("OVER CURRENT    ");
    lcd.setCursor(0, 1);
    lcd.print("CHECK DEVICE    ");

  } else {

    lcd.setCursor(0, 0);
    lcd.printf("T:%d H:%d       ", t, h);
    lcd.setCursor(0, 1);
    lcd.printf("L:%.1f F:%.1f  ", cL, cF);
  }

  xSemaphoreGive(lcdMutex);
}

//======================================================
// SEND WEB — local snapshot, no mutex held during HTTP
//======================================================
void sendWeb() {

  if (WiFi.status() != WL_CONNECTED) return;

  // Snapshot
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  float snapT   = temperature;
  float snapH   = humidity;
  float snapCL  = currentL;
  float snapCF  = currentF;
  bool  snapP   = presence;
  bool  snapL   = lightState;
  bool  snapF   = fanState;
  xSemaphoreGive(stateMutex);

  StaticJsonDocument<256> doc;
  doc["temperature"] = snapT;
  doc["humidity"]    = snapH;
  doc["currentL"]    = snapCL;
  doc["currentF"]    = snapCF;
  doc["presence"]    = snapP;
  doc["light"]       = snapL;
  doc["fan"]         = snapF;

  String json;
  serializeJson(doc, json);

  HTTPClient http;
  http.setTimeout(2000);
  http.begin(API_DATA);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(json);

  Serial.print("POST = ");
  Serial.println(code);

  http.end();
}

//======================================================
// GET CONTROL — apply relay after releasing mutex
//======================================================
void getWebControl() {

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(2000);
  http.begin(API_CONTROL);

  int code = http.GET();

  if (code == 200) {

    String payload = http.getString();

    StaticJsonDocument<256> doc;

    if (deserializeJson(doc, payload) == DeserializationError::Ok) {

      bool newLight = lightState;
      bool newFan   = fanState;

      if (doc.containsKey("light")) newLight = doc["light"];
      if (doc.containsKey("fan"))   newFan   = doc["fan"];

      xSemaphoreTake(stateMutex, portMAX_DELAY);
      lightState = newLight;
      fanState   = newFan;
      xSemaphoreGive(stateMutex);

      // Apply relay outside mutex
      digitalWrite(RELAY_LIGHT, newLight);
      digitalWrite(RELAY_FAN,   newFan);
    }
  }

  http.end();
}

//======================================================
// AI INFERENCE — runs on Core 1 in taskAI
//======================================================
void runAIInference() {

  static int32_t raw_buffer[256];
  size_t bytes_read;
  int    samples_collected = 0;

  memset(sampleBuffer, 0, sizeof(sampleBuffer));

  // Collect samples
  while (samples_collected < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {

    xSemaphoreTake(i2sMutex, portMAX_DELAY);

    esp_err_t err = i2s_read(
      I2S_PORT,
      raw_buffer,
      sizeof(raw_buffer),
      &bytes_read,
      pdMS_TO_TICKS(100)   // timeout 100ms, not portMAX_DELAY
    );

    xSemaphoreGive(i2sMutex);

    if (err != ESP_OK || bytes_read == 0) {
      Serial.println("I2S read failed, skip");
      return;
    }

    int samples = bytes_read / 4;

    if (samples > (EI_CLASSIFIER_RAW_SAMPLE_COUNT - samples_collected)) {
      samples = EI_CLASSIFIER_RAW_SAMPLE_COUNT - samples_collected;
    }

    for (int i = 0; i < samples; i++) {

      int16_t val = (int16_t)(raw_buffer[i] >> 14);

      if (abs(val) < 50) val = 0;

      sampleBuffer[samples_collected++] = val * 4;
    }
  }

  // Run classifier
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data     = &ei_get_data;

  ei_impulse_result_t result = {0};

  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

  if (err != EI_IMPULSE_OK) return;

  // Find best label
  int   best   = 0;
  float maxVal = 0;

  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > maxVal) {
      maxVal = result.classification[i].value;
      best   = i;
    }
  }

  if (maxVal < 0.75f) return;

  const char* label = result.classification[best].label;

  Serial.print("AI: ");
  Serial.print(label);
  Serial.print(" (");
  Serial.print(maxVal);
  Serial.println(")");

  if (strcmp(label, "Tinhieunhieu") == 0) return;

  // Update state
  xSemaphoreTake(stateMutex, portMAX_DELAY);

  if      (strcmp(label, "Batden")  == 0) lightState = true;
  else if (strcmp(label, "Tatden")  == 0) lightState = false;
  else if (strcmp(label, "Batquat") == 0) fanState   = true;
  else if (strcmp(label, "Tatquat") == 0) fanState   = false;

  // Snapshot INSIDE mutex before giving it up
  bool lSnap = lightState;
  bool fSnap = fanState;

  xSemaphoreGive(stateMutex);

  // Apply relay with snapshot values
  digitalWrite(RELAY_LIGHT, lSnap);
  digitalWrite(RELAY_FAN,   fSnap);
}

//======================================================
// EI CALLBACK
//======================================================
int ei_get_data(size_t offset, size_t length, float* out_ptr) {

  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (float)sampleBuffer[offset + i] / 32768.0f;
  }

  return 0;
}

//======================================================
// I2S SETUP
//======================================================
void setupI2S() {

  i2s_config_t config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = 16000,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,
    .dma_buf_len          = 512,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num     = I2S_SCK,
    .ws_io_num      = I2S_WS,
    .data_out_num   = I2S_PIN_NO_CHANGE,
    .data_in_num    = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

//======================================================
// TASK: SENSOR + OVER CURRENT SAFETY (Core 0, Priority 3)
// Chạy mỗi 1s, đọc DHT + ACS712, kiểm tra quá dòng
//======================================================
void taskSensor(void* pv) {

  static uint32_t lastBlink = 0;

  while (1) {

    readSensor();

    // Snapshot sau khi readSensor cập nhật xong
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    float cL = currentL;
    float cF = currentF;
    xSemaphoreGive(stateMutex);

    if (cL > CURRENT_THRESHOLD || cF > CURRENT_THRESHOLD) {

      // Tắt relay NGAY
      digitalWrite(RELAY_LIGHT, LOW);
      digitalWrite(RELAY_FAN,   LOW);

      xSemaphoreTake(stateMutex, portMAX_DELAY);
      overCurrent = true;
      lightState  = false;
      fanState    = false;
      xSemaphoreGive(stateMutex);

      Serial.println("!!! OVER CURRENT !!!");

      // Nháy LED cảnh báo
      if (millis() - lastBlink > 500) {
        lastBlink = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      }

    } else {

      xSemaphoreTake(stateMutex, portMAX_DELAY);
      overCurrent = false;
      xSemaphoreGive(stateMutex);

      digitalWrite(LED_PIN, LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

//======================================================
// TASK: LCD (Core 0, Priority 1)
// Cập nhật màn hình mỗi 500ms
//======================================================
void taskLCD(void* pv) {

  while (1) {

    updateLCD();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

//======================================================
// TASK: WEB SEND + CONTROL (Core 0, Priority 1)
// Gửi data + nhận lệnh điều khiển mỗi 5s
//======================================================
void taskWeb(void* pv) {

  // Chờ WiFi ổn định trước
  vTaskDelay(pdMS_TO_TICKS(2000));

  while (1) {

    // Reconnect nếu mất WiFi
    if (WiFi.status() != WL_CONNECTED) {

      Serial.println("WiFi lost, reconnecting...");

      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(1000));
      WiFi.begin(ssid, pass);

      int retry = 0;

      while (WiFi.status() != WL_CONNECTED && retry < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
      }
    }

    sendWeb();
    getWebControl();

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

//======================================================
// TASK: AI INFERENCE (Core 1, Priority 2)
// Chỉ chạy khi có người và không quá dòng
//======================================================
void taskAI(void* pv) {

  while (1) {

    // Snapshot để check điều kiện
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool p  = presence;
    bool oc = overCurrent;
    xSemaphoreGive(stateMutex);

    if (p && !oc) {

      runAIInference();

    } else {

      // Không chạy AI thì yield CPU
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

//======================================================
// TASK: RADAR (Core 1, Priority 3)
// Phát hiện người vào/ra mỗi 100ms
//======================================================
void taskRadar(void* pv) {

  bool lastRadar = false;

  while (1) {

    bool radar = digitalRead(RADAR_PIN);

    //--------------------------------------------------
    // NGƯỜI VÀO
    //--------------------------------------------------
    if (radar && !lastRadar) {

      Serial.println(">>> PERSON DETECTED");

      int ldr = readLDR();

      bool shouldLight = (ldr < LDR_THRESHOLD);

      xSemaphoreTake(stateMutex, portMAX_DELAY);
      presence   = true;
      lightState = shouldLight;
      xSemaphoreGive(stateMutex);

      digitalWrite(RELAY_LIGHT, shouldLight);

      xSemaphoreTake(lcdMutex, portMAX_DELAY);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PERSON DETECT");
      xSemaphoreGive(lcdMutex);

      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //--------------------------------------------------
    // NGƯỜI RA
    //--------------------------------------------------
    if (!radar && lastRadar) {

      Serial.println(">>> PERSON LEFT");

      xSemaphoreTake(stateMutex, portMAX_DELAY);
      presence   = false;
      lightState = false;
      fanState   = false;
      xSemaphoreGive(stateMutex);

      digitalWrite(RELAY_LIGHT, LOW);
      digitalWrite(RELAY_FAN,   LOW);

      xSemaphoreTake(lcdMutex, portMAX_DELAY);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SLEEP MODE");
      xSemaphoreGive(lcdMutex);
    }

    lastRadar = radar;

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

//======================================================
// SETUP
//======================================================
void setup() {

  Serial.begin(115200);

  //----------------------------------------------------
  // PIN SETUP
  //----------------------------------------------------
  pinMode(LED_PIN,     OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_FAN,   OUTPUT);
  pinMode(RADAR_PIN,   INPUT);

  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(RELAY_FAN,   LOW);
  digitalWrite(LED_PIN,     LOW);

  //----------------------------------------------------
  // I2C + LCD
  //----------------------------------------------------
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM START");

  //----------------------------------------------------
  // SENSOR
  //----------------------------------------------------
  dht.begin();
  delay(1000);

  acsL.autoMidPoint();
  acsF.autoMidPoint();

  //----------------------------------------------------
  // I2S + AI
  //----------------------------------------------------
  setupI2S();

  //----------------------------------------------------
  // WIFI
  //----------------------------------------------------
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  connectWiFi();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi OK");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  delay(1000);

  //----------------------------------------------------
  // MUTEX — PHẢI tạo TRƯỚC khi tạo tasks
  //----------------------------------------------------
  stateMutex = xSemaphoreCreateMutex();
  lcdMutex   = xSemaphoreCreateMutex();
  i2sMutex   = xSemaphoreCreateMutex();

  //----------------------------------------------------
  // CREATE TASKS
  //
  // Core 0: taskSensor, taskLCD, taskWeb
  // Core 1: taskAI, taskRadar, loop (idle)
  //
  // Priority: Safety(3) > Radar(3) > AI(2) > Sensor(2) > LCD(1) = Web(1)
  //----------------------------------------------------

  xTaskCreatePinnedToCore(
    taskSensor,   // function
    "SENSOR",     // name
    4096,         // stack bytes
    NULL,         // param
    3,            // priority
    NULL,         // handle
    0             // core
  );

  xTaskCreatePinnedToCore(
    taskLCD,
    "LCD",
    4096,         // tăng từ 2048 lên 4096
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    taskWeb,
    "WEB",
    6144,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    taskAI,
    "AI",
    16384,
    NULL,
    2,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskRadar,
    "RADAR",
    4096,
    NULL,
    3,
    NULL,
    1
  );

  //----------------------------------------------------
  // DONE
  //----------------------------------------------------
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM READY");

  Serial.println("=== ALL TASKS STARTED ===");
}

//======================================================
// LOOP — idle, không làm gì
//======================================================
void loop() {

  vTaskDelay(pdMS_TO_TICKS(1000));
}
