//======================================================
// SMART ROOM AI SYSTEM - FIXED VERSION
//======================================================

//#define EI_CLASSIFIER_ALLOCATION_STATIC 1
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
char ssid[] = "quamondi";
char pass[] = "12345678";

//======================================================
// API
//======================================================
const char* API_DATA =
"http://10.103.107.32:5000/api/data";

const char* API_CONTROL =
"http://10.103.107.32:5000/api/control";

//======================================================
// I2S
//======================================================
#define I2S_WS     25
#define I2S_SCK    32
#define I2S_SD     33
#define I2S_PORT   I2S_NUM_0

//======================================================
// PIN
//======================================================
#define LED_PIN        14

#define RELAY_LIGHT    26
#define RELAY_FAN      27

#define RADAR_PIN      16
#define LDR_PIN        36

#define DHTPIN         4
#define DHTTYPE        DHT22

#define ACS_LIGHT      35
#define ACS_FAN        34

//======================================================
// THRESHOLD
//======================================================
#define LDR_THRESHOLD       1500
#define CURRENT_THRESHOLD   10000

//======================================================
// OBJECT
//======================================================
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27,16,2);

ACS712 acsL(ACS_LIGHT,3.3,4095,185);
ACS712 acsF(ACS_FAN,3.3,4095,185);

//======================================================
// AI BUFFER
//======================================================
static signed short sampleBuffer[
EI_CLASSIFIER_RAW_SAMPLE_COUNT
];

//======================================================
// STATE
//======================================================
bool presence = false;

bool systemReady = false;

bool overCurrent = false;

bool lightState = false;
bool fanState = false;

bool manualLight = false;
bool manualFan = false;

//======================================================
// TIMER
//======================================================
uint32_t lastRadarDetect = 0;

uint32_t lastWebSend = 0;

uint32_t lastLedBlink = 0;

uint32_t lastSensorRead = 0;

//======================================================
// SENSOR DATA
//======================================================
float temperature = 0;
float humidity = 0;

float currentL = 0;
float currentF = 0;

//======================================================
// FUNCTION
//======================================================
void setupI2S();

void connectWiFi();

int readLDR();

void runAIInference();

void updateLCD();

void readSensor();

void sendWeb();

void getWebControl();

//======================================================
// WIFI CONNECT
//======================================================
void connectWiFi(){

  if(WiFi.status() == WL_CONNECTED){
    return;
  }

  Serial.println("Connecting WiFi...");

  WiFi.mode(WIFI_STA);

  WiFi.setSleep(false);

  WiFi.begin(ssid, pass);

  int retry = 0;

  while(WiFi.status() != WL_CONNECTED){

    delay(500);

    Serial.print(".");

    retry++;

    if(retry > 40){

      Serial.println("\nWiFi Retry...");
      retry = 0;

      WiFi.disconnect();

      delay(1000);

      WiFi.begin(ssid, pass);
    }
  }

  Serial.println("\nWiFi Connected");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  systemReady = true;
}

//======================================================
// SETUP
//======================================================
void setup(){

  Serial.begin(115200);

  pinMode(LED_PIN,OUTPUT);

  pinMode(RELAY_LIGHT,OUTPUT);
  pinMode(RELAY_FAN,OUTPUT);

  pinMode(RADAR_PIN,INPUT);

  digitalWrite(RELAY_LIGHT,LOW);
  digitalWrite(RELAY_FAN,LOW);

  // LCD
  Wire.begin(21,22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("SYSTEM START");

  // SENSOR
  dht.begin();

  acsL.autoMidPoint();
  acsF.autoMidPoint();

  // AI
  setupI2S();

  delay(1000);

  lcd.clear();
}

//======================================================
// LOOP
//======================================================
void loop(){

  bool radar = digitalRead(RADAR_PIN);

  //====================================================
  // PERSON DETECT
  //====================================================
  if(radar){

    //--------------------------------------------------
    // WAKE UP
    //--------------------------------------------------
    if(!presence){

      presence = true;

      Serial.println(">>> PERSON DETECTED");

      lcd.clear();

      lcd.setCursor(0,0);
      lcd.print("PERSON DETECT");

      //------------------------------------------------
      // READ LDR 1 TIME
      //------------------------------------------------
      delay(1000);

      int ldr = readLDR();

      Serial.print("LDR = ");
      Serial.println(ldr);

      if(!manualLight){

        if(ldr < LDR_THRESHOLD){

          lightState = true;

          Serial.println("DARK -> LIGHT ON");

        }else{

          lightState = false;

          Serial.println("BRIGHT -> LIGHT OFF");
        }
      }

      digitalWrite(RELAY_LIGHT,lightState);

      //------------------------------------------------
      // WIFI FIRST
      //------------------------------------------------
      connectWiFi();

      lcd.clear();

      lcd.setCursor(0,0);
      lcd.print("SYSTEM READY");

      delay(500);
    }

    lastRadarDetect = millis();

    //--------------------------------------------------
    // SYSTEM ONLY RUN WHEN WIFI READY
    //--------------------------------------------------
    if(systemReady){

      //------------------------------------------------
      // AI 3s
      //------------------------------------------------
      Serial.println("=== AI START ===");

      uint32_t aiStart = millis();

      while(millis() - aiStart < 3000){

        runAIInference();

        if(!digitalRead(RADAR_PIN)){
          break;
        }
      }

      Serial.println("=== AI END ===");

      //------------------------------------------------
      // SENSOR EVERY 1s
      //------------------------------------------------
      if(millis() - lastSensorRead > 1000){

        lastSensorRead = millis();

        readSensor();

        //------------------------------------------------
        // OVER CURRENT
        //------------------------------------------------
        if(currentL * 1000 > CURRENT_THRESHOLD ||
           currentF * 1000 > CURRENT_THRESHOLD){

          overCurrent = true;

          Serial.println("!!! OVER CURRENT !!!");

          lightState = false;
          fanState = false;

          digitalWrite(RELAY_LIGHT,LOW);
          digitalWrite(RELAY_FAN,LOW);

          //------------------------------------------------
          // LED BLINK
          //------------------------------------------------
          if(millis() - lastLedBlink > 500){

            lastLedBlink = millis();

            digitalWrite(
              LED_PIN,
              !digitalRead(LED_PIN)
            );
          }

          lcd.clear();

          lcd.setCursor(0,0);
          lcd.print("OVER CURRENT");

          lcd.setCursor(0,1);
          lcd.print("CHECK DEVICE");
        }
        else{

          overCurrent = false;

          digitalWrite(LED_PIN,LOW);

          updateLCD();
        }
      }

      //------------------------------------------------
      // WEB EVERY 5s
      //------------------------------------------------
      if(millis() - lastWebSend > 5000){

        lastWebSend = millis();

        if(WiFi.status() == WL_CONNECTED){

          sendWeb();

          getWebControl();
        }
      }
    }
  }

  //====================================================
  // NO PERSON
  //====================================================
  else{

    if(presence &&
       millis() - lastRadarDetect > 2000){

      presence = false;

      Serial.println(">>> PERSON LEFT");

      manualLight = false;
      manualFan = false;

      lightState = false;
      fanState = false;

      digitalWrite(RELAY_LIGHT,LOW);
      digitalWrite(RELAY_FAN,LOW);

      lcd.clear();

      lcd.setCursor(0,0);
      lcd.print("SLEEP MODE");
    }

    //--------------------------------------------------
    // SEND WEB 5s
    //--------------------------------------------------
    if(millis() - lastWebSend > 5000){

      lastWebSend = millis();

      if(WiFi.status() == WL_CONNECTED){

        sendWeb();
      }
    }
  }
}

//======================================================
// READ SENSOR
//======================================================
void readSensor(){

  int curL = acsL.mA_AC(20,1);
  int curF = acsF.mA_AC(20,1);

  if(abs(curL) < 200) curL = 0;
  if(abs(curF) < 200) curF = 0;

  currentL = curL / 1000.0;
  currentF = curF / 1000.0;

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if(isnan(temperature)) temperature = 0;
  if(isnan(humidity)) humidity = 0;
}

//======================================================
// LDR
//======================================================
int readLDR(){

  int sum = 0;

  for(int i=0;i<5;i++){

    sum += analogRead(LDR_PIN);

    delay(2);
  }

  return sum / 5;
}

//======================================================
// LCD
//======================================================
void updateLCD(){

  lcd.setCursor(0,0);

  lcd.printf(
    "T:%d H:%d   ",
    (int)temperature,
    (int)humidity
  );

  lcd.setCursor(0,1);

  lcd.printf(
    "L:%.1f F:%.1f ",
    currentL,
    currentF
  );
}

//======================================================
// SEND WEB
//======================================================
void sendWeb(){

  HTTPClient http;

  http.setTimeout(1000);

  http.begin(API_DATA);

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  StaticJsonDocument<256> doc;

  doc["temperature"] = temperature;
  doc["humidity"] = humidity;

  doc["currentL"] = currentL;
  doc["currentF"] = currentF;

  doc["presence"] = presence;

  doc["light"] = lightState;
  doc["fan"] = fanState;

  String json;

  serializeJson(doc,json);

  Serial.println(json);

  int code = http.POST(json);

  Serial.print("HTTP CODE = ");
  Serial.println(code);

  http.end();
}

//======================================================
// GET CONTROL
//======================================================
void getWebControl(){

  HTTPClient http;

  http.setTimeout(1000);

  http.begin(API_CONTROL);

  int code = http.GET();

  if(code == 200){

    String payload = http.getString();

    StaticJsonDocument<256> doc;

    DeserializationError err =
    deserializeJson(doc,payload);

    if(!err){

      if(doc.containsKey("light")){

        lightState = doc["light"];

        digitalWrite(
          RELAY_LIGHT,
          lightState
        );
      }

      if(doc.containsKey("fan")){

        fanState = doc["fan"];

        digitalWrite(
          RELAY_FAN,
          fanState
        );
      }
    }
  }

  http.end();
}

//======================================================
// AI
//======================================================
void runAIInference(){

  static int32_t raw_buffer[256];

  size_t bytes_read;

  int samples_collected = 0;

  memset(
    sampleBuffer,
    0,
    sizeof(sampleBuffer)
  );

  while(samples_collected <
        EI_CLASSIFIER_RAW_SAMPLE_COUNT){

    i2s_read(
      I2S_PORT,
      (void*)raw_buffer,
      sizeof(raw_buffer),
      &bytes_read,
      portMAX_DELAY
    );

    int samples = bytes_read / 4;

    if(samples >
      (EI_CLASSIFIER_RAW_SAMPLE_COUNT -
       samples_collected)){

      samples =
      EI_CLASSIFIER_RAW_SAMPLE_COUNT -
      samples_collected;
    }

    for(int i=0;i<samples;i++){

      int16_t val =
      (int16_t)(raw_buffer[i] >> 14);

      if(abs(val) < 50){
        val = 0;
      }

      sampleBuffer[
        samples_collected++
      ] = val * 4;
    }
  }

  signal_t signal;

  signal.total_length =
  EI_CLASSIFIER_RAW_SAMPLE_COUNT;

  signal.get_data = &ei_get_data;

  ei_impulse_result_t result = {0};

  if(run_classifier(
      &signal,
      &result,
      false
    ) == EI_IMPULSE_OK){

    int best = 0;

    float maxVal = 0;

    for(int i=0;
        i<EI_CLASSIFIER_LABEL_COUNT;
        i++){

      if(result.classification[i].value
         > maxVal){

        maxVal =
        result.classification[i].value;

        best = i;
      }
    }

    const char* label =
    result.classification[best].label;

    if(maxVal > 0.75){

      if(strcmp(label,"Tinhieunhieu")==0){
        return;
      }

      Serial.print("AI: ");
      Serial.println(label);

      if(strcmp(label,"Batden")==0){

        lightState = true;
      }

      else if(strcmp(label,"Tatden")==0){

        lightState = false;
      }

      else if(strcmp(label,"Batquat")==0){

        fanState = true;
      }

      else if(strcmp(label,"Tatquat")==0){

        fanState = false;
      }

      digitalWrite(
        RELAY_LIGHT,
        lightState
      );

      digitalWrite(
        RELAY_FAN,
        fanState
      );
    }
  }
}

//======================================================
// EI CALLBACK
//======================================================
int ei_get_data(
  size_t offset,
  size_t length,
  float *out_ptr
){

  for(size_t i=0;i<length;i++){

    out_ptr[i] =
    (float)sampleBuffer[offset+i]
    / 32768.0f;
  }

  return 0;
}

//======================================================
// I2S
//======================================================
void setupI2S(){

  i2s_config_t config = {

    .mode = (i2s_mode_t)(
      I2S_MODE_MASTER |
      I2S_MODE_RX
    ),

    .sample_rate = 16000,

    .bits_per_sample =
    I2S_BITS_PER_SAMPLE_32BIT,

    .channel_format =
    I2S_CHANNEL_FMT_ONLY_RIGHT,

    .communication_format =
    (i2s_comm_format_t)(
      I2S_COMM_FORMAT_I2S |
      I2S_COMM_FORMAT_I2S_MSB
    ),

    .intr_alloc_flags =
    ESP_INTR_FLAG_LEVEL1,

    .dma_buf_count = 8,

    .dma_buf_len = 512
  };

  i2s_pin_config_t pin_config = {

    .bck_io_num = I2S_SCK,

    .ws_io_num = I2S_WS,

    .data_out_num =
    I2S_PIN_NO_CHANGE,

    .data_in_num = I2S_SD
  };

  i2s_driver_install(
    I2S_PORT,
    &config,
    0,
    NULL
  );

  i2s_set_pin(
    I2S_PORT,
    &pin_config
  );

  i2s_zero_dma_buffer(I2S_PORT);
}