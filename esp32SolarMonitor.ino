#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define ADS_SDA_PIN 23
#define ADS_SCL_PIN 22

#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define NOISE_THRESHOLD 0.0015
#define COIL_CONVERSION_RATIO 29.5
#define HOUSEHOLD_VOLTAGE 246   //!!!!!!!!!!!!!!! change this when installing.
#define MQTT_MAX_PACKET_SIZE 3500

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

TwoWire I2C_Sensors = TwoWire(0);   // Bus 0 for ADS1115 modules
TwoWire I2C_Display = TwoWire(1);   // Bus 1 for OLED

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_Display, OLED_RESET);

Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;
Adafruit_ADS1115 ads3;
#define ADS1_ALERT_PIN 4
#define ADS2_ALERT_PIN 21
#define ADS3_ALERT_PIN 2
#define ADS1_ADDR 0x49  //  ALRT pin to PWR
#define ADS2_ADDR 0x48  //  ALRT pin to GND
#define ADS3_ADDR 0x4A  //  ALRT pin to SDA

volatile bool ADS1_Ready = false;
volatile bool ADS2_Ready = false;
volatile bool ADS3_Ready = false;

double ADS1_sumSquares = 0;
unsigned int ADS1_sampleCount = 0;
double ADS1_totalWh = 0.0;
double ADS1_energy = 0.0;
unsigned int ADS1_energy_count = 0;

double ADS2_sumSquares = 0;
unsigned int ADS2_sampleCount = 0;
double ADS2_totalWh = 0.0;
double ADS2_energy = 0.0;
unsigned int ADS2_energy_count = 0;

double ADS3_sumSquares = 0;
unsigned int ADS3_sampleCount = 0;
double ADS3_totalWh = 0.0;
double ADS3_energy = 0.0;
unsigned int ADS3_energy_count = 0;

unsigned int lastSampleMS = 0;

const char* ssid = "************";
const char* password = "***********";
const char* mqtt_server   = "192.168.1.10";
const int   mqtt_port     = 1883;
const char* mqtt_user     = "mqtt";
const char* mqtt_password = "**********";
const char* mqtt_client_name = "SOLAR_WATCHER_5000";
char* mqtt_status = "";

const unsigned int PeriodForDataDeliveryMS = 10000;  // deliver data to HA every 10 seconds
const unsigned short NumberOfPeriodsInAnHour = 360;  // this must be (60*60/(PeriodForDataDeliveryMS/1000))
unsigned int PreviousDataDeliveryMS = 0;


struct SensorReading {
  unsigned int ads1SampleCount;
  double ads1SumSquares;
  unsigned int ads2SampleCount;
  double ads2SumSquares;
  unsigned int ads3SampleCount;
  double ads3SumSquares;
};

void IRAM_ATTR ADS1_Alrt() {
  ADS1_Ready = true;
}
void IRAM_ATTR ADS2_Alrt() {
  ADS2_Ready = true;
}
void IRAM_ATTR ADS3_Alrt() {
  ADS3_Ready = true;
}

const char* getSignalQuality() {
  if (WiFi.status() != WL_CONNECTED) {
    return "No Conn";
  }
  int rssi = WiFi.RSSI();
  if (rssi >= -50) return "Excellent";
  if (rssi >= -70) return "Good";
  if (rssi >= -80) return "Weak";
  return "Bad";
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  if (WiFi.status() == WL_DISCONNECTED) {
    WiFi.reconnect();
  } else {
    WiFi.begin(ssid, password);
  }
  vTaskDelay(pdMS_TO_TICKS(1000));

  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      setupTime();
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}

void connectWiFi() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(2, 0);
  display.println("Connecting");
  display.setCursor(2, 18);
  display.println("to WiFi");
  display.setCursor(2, 42);
  display.println(ssid);
  display.display();
  delay(100);

  if (! ensureWiFi()) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(2, 0);
    display.println("Wifi Error");
    display.display();
    delay(5000);
    return;
  }
}

void setupTime() {
    
  configTzTime("EST5EDT,M3.2.0/2,M11.1.0/2","pool.ntp.org","time.nist.gov");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(2, 0);
    display.println("NTP Error");
    display.display();
    delay(5000);
  }
}

void splash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(12, 0);
  display.println("Solar");
  display.setCursor(12, 18);
  display.println("Watcher");
  display.setCursor(12, 42);
  display.println("5000");
  display.display();
  delay(3500);
}

void configureADS1() {
  if (!ads1.begin(ADS1_ADDR, &I2C_Sensors)) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(2, 0);
    display.println("ADS1 error");
    display.display();
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.println("AD1 connected.");
  display.display();

  ads1.setGain(GAIN_TWO);
  ads1.setDataRate(RATE_ADS1115_860SPS);
  ads1.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/true);
  pinMode(ADS1_ALERT_PIN, INPUT_PULLUP);
  delay(10);
  attachInterrupt(digitalPinToInterrupt(ADS1_ALERT_PIN), ADS1_Alrt, FALLING);
}

void configureADS2() {
  if (!ads2.begin(ADS2_ADDR, &I2C_Sensors)) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(2, 0);
    display.println("ADS2 error");
    display.display();
    while (1);
  }
  display.setTextSize(1);
  display.setCursor(2, 18);
  display.println("AD2 connected.");
  display.display();

  ads2.setGain(GAIN_TWO);
  ads2.setDataRate(RATE_ADS1115_860SPS);
  ads2.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/true);
  pinMode(ADS2_ALERT_PIN, INPUT_PULLUP);
  delay(10);
  attachInterrupt(digitalPinToInterrupt(ADS2_ALERT_PIN), ADS2_Alrt, FALLING);
}

void configureADS3() {
  if (!ads3.begin(ADS3_ADDR, &I2C_Sensors)) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(2, 0);
    display.println("ADS3 error");
    display.display();
    while (1);
  }
  display.setTextSize(1);
  display.setCursor(2, 42);
  display.println("AD3 connected.");
  display.setCursor(2, 53);
  display.println(I2C_Sensors.getClock());
  display.display();

  ads3.setGain(GAIN_TWO);
  ads3.setDataRate(RATE_ADS1115_860SPS);
  ads3.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/true);
  pinMode(ADS3_ALERT_PIN, INPUT_PULLUP);
  delay(10);
  attachInterrupt(digitalPinToInterrupt(ADS3_ALERT_PIN), ADS3_Alrt, FALLING);
}

// void publishMQTTDiscovery() {
//     mqttClient.publish( "homeassistant/sensor/solar_inverter1_power/config",
//         R"({"name":"Inverter 1 Power","state_topic":"solar/inverter1/power","unit_of_measurement":"W","device_class":"power","state_class":"measurement","unique_id":"solar_inverter1_power","device":{  "identifiers":["solar_monitor"],  "name":"Solar Monitor",  "manufacturer":"Custom",  "model":"SOLAR WATCHER 5000"}})", true);
//     mqttClient.loop();
//     mqttClient.publish("homeassistant/sensor/solar_inverter2_power/config",
//         R"({"name":"Inverter 2 Power","state_topic":"solar/inverter2/power","unit_of_measurement":"W","device_class":"power","state_class":"measurement","unique_id":"solar_inverter2_power","device":{  "identifiers":["solar_monitor"],  "name":"Solar Monitor",  "manufacturer":"Custom",  "model":"SOLAR WATCHER 5000"}})", true);
//     mqttClient.loop();
//     mqttClient.publish("homeassistant/sensor/solar_inverter3_power/config",
//         R"({"name":"Inverter 3 Power","state_topic":"solar/inverter3/power","unit_of_measurement":"W","device_class":"power","state_class":"measurement","unique_id":"solar_inverter3_power","device":{  "identifiers":["solar_monitor"],  "name":"Solar Monitor",  "manufacturer":"Custom",  "model":"SOLAR WATCHER 5000"}})", true);
//     mqttClient.loop();
// }

void publishMQTTDiscovery() {

  if (!ensureWiFi()) return;
  if (!ensureMQTT()) return;

  const char* payload = R"({
    "origin": {
      "name": "solar-monitor",
      "sw_version": "1.0.0"
    },
    "device": {
      "identifiers": ["solar_monitor"],
      "name": "Solar Monitor",
      "manufacturer": "SwenCo",
      "model": "SOLAR WATCHER 5000",
      "sw_version": "1.0.0"
    },
    "availability": [{
      "topic": "solar/monitor/status"
    }],
    "components": {
      "inverter1_power": {
        "platform": "sensor",
        "name": "Inverter 1 Power",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.inverter1 }}",
        "unit_of_measurement": "W",
        "device_class": "power",
        "state_class": "measurement",
        "unique_id": "solar_monitor_inverter1_power"
      },
      "inverter2_power": {
        "platform": "sensor",
        "name": "Inverter 2 Power",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.inverter2 }}",
        "unit_of_measurement": "W",
        "device_class": "power",
        "state_class": "measurement",
        "unique_id": "solar_monitor_inverter2_power"
      },
      "inverter3_power": {
        "platform": "sensor",
        "name": "Inverter 3 Power",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.inverter3 }}",
        "unit_of_measurement": "W",
        "device_class": "power",
        "state_class": "measurement",
        "unique_id": "solar_monitor_inverter3_power"
      },
      "total_power": {
        "platform": "sensor",
        "name": "Total Solar Power",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.total }}",
        "unit_of_measurement": "W",
        "device_class": "power",
        "state_class": "measurement",
        "unique_id": "solar_monitor_total_power"
      },
      "wifi_signal": {
        "platform": "sensor",
        "name": "WiFi Signal",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.wifi }}",
        "unit_of_measurement": "dBm",
        "device_class": "signal_strength",
        "entity_category": "diagnostic",
        "unique_id": "solar_monitor_wifi_signal"
      },
      "uptime": {
        "platform": "sensor",
        "name": "Uptime",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.uptime }}",
        "unit_of_measurement": "s",
        "entity_category": "diagnostic",
        "unique_id": "solar_monitor_uptime"
      },
      "esp_temperature": {
        "platform": "sensor",
        "name": "ESP32 Temperature",
        "state_topic": "solar/monitor/state",
        "value_template": "{{ value_json.temp }}",
        "unit_of_measurement": "°C",
        "device_class": "temperature",
        "entity_category": "diagnostic",
        "unique_id": "solar_monitor_temp"
      }
    }
  })";

  bool ok = mqttClient.publish("homeassistant/device/solar_monitor/config", payload, true);

  if (!ok) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(2, 20);
    display.println("MQTT config error");
    display.display();
    delay(10000);
  }
  else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(2, 20);
    display.println("MQTT config");
    display.setCursor(2, 40);
    display.println("published OK");
    display.display();
    delay(2000);
  }

  mqttClient.loop();
}

void setMqttStatus() {
  switch (mqttClient.state()) {
    case 0:
      mqtt_status = "connected";
      break;
    case -1:
      mqtt_status = "timeout";
      break;
    case -2:
      mqtt_status = "network failed";
      break;
    case -3:
      mqtt_status = "connection lost";
      break;
    case -4:
      mqtt_status = "connect failed";
      break;
    case -5:
      mqtt_status = "disconnected";
      break;
    case 1:
      mqtt_status = "bad protocol";
      break;
    case 2:
      mqtt_status = "bad client ID";
      break;
    case 3:
      mqtt_status = "broker unavailable";
      break;
    case 4:
      mqtt_status = "bad credentials";
      break;
    case 5:
      mqtt_status = "unauthorized";
      break;
    default:
      mqtt_status = "unknown";
      break;
  }
}

bool ensureMQTT() {
  if (mqttClient.connected()) {
        return true;
  }
  if (mqttClient.connect(mqtt_client_name, mqtt_user, mqtt_password,"solar/monitor/status", 0, true, "offline")) {
    delay(10);
    mqttClient.loop();
    delay(5);
    mqttClient.loop();
    setMqttStatus();
    mqttClient.loop();
    publishMQTTDiscovery();
    mqttClient.loop();
    return true;
  }
  setMqttStatus();
  return false;
}

void sendToHomeAssistant(const float ads1Watts, const float ads2Watts, const float ads3Watts) {
  if (!ensureWiFi()) {
    return;
  }
  if (!ensureMQTT()) {
    return;
  }
  float total = ads1Watts + ads2Watts + ads3Watts;
  char payload[512];

  long uptime = millis() / 1000;
  int wifi = WiFi.RSSI();
  float temp = temperatureRead();

  snprintf(payload, sizeof(payload),
           R"({"inverter1":%.2f,"inverter2":%.2f,"inverter3":%.2f,"total":%.2f,"wifi":%d,"uptime":%ld,"temp":%.1f})",
           ads1Watts, ads2Watts, ads3Watts, total, wifi, uptime, temp);

  mqttClient.publish("solar/monitor/state", payload, true);
  mqttClient.loop();
  mqttClient.publish("solar/monitor/status", "online", true);

  // snprintf(payload, sizeof(payload), "%.2f", ads1Watts);
  // mqttClient.publish("solar/inverter1/power", payload, true);
  // mqttClient.loop();
  // snprintf(payload, sizeof(payload), "%.2f", ads2Watts);
  // mqttClient.publish("solar/inverter2/power", payload, true);
  // mqttClient.loop();
  // snprintf(payload, sizeof(payload), "%.2f", ads3Watts);
  // mqttClient.publish("solar/inverter3/power", payload, true);
  mqttClient.loop();
}

void ItsBeenASecond(const SensorReading &reading) {
    float ADS1_rmsVoltage = sqrt(reading.ads1SumSquares / reading.ads1SampleCount);
    if (ADS1_rmsVoltage <= NOISE_THRESHOLD) {  // just noise.
      ADS1_rmsVoltage = 0;
    }
    float ADS1_amps = ADS1_rmsVoltage * COIL_CONVERSION_RATIO;
    float ADS1_watts = ADS1_amps * HOUSEHOLD_VOLTAGE;
    ADS1_energy += ADS1_watts;
    ADS1_energy_count++;

    float ADS2_rmsVoltage = sqrt(reading.ads2SumSquares / reading.ads2SampleCount);
    if (ADS2_rmsVoltage <= NOISE_THRESHOLD) {  // just noise.
      ADS2_rmsVoltage = 0;
    }
    float ADS2_amps = ADS2_rmsVoltage * COIL_CONVERSION_RATIO;
    float ADS2_watts = ADS2_amps * HOUSEHOLD_VOLTAGE;
    ADS2_energy += ADS2_watts;
    ADS2_energy_count++;

    float ADS3_rmsVoltage = sqrt(reading.ads3SumSquares / reading.ads3SampleCount);
    if (ADS3_rmsVoltage <= NOISE_THRESHOLD) {  // just noise.
      ADS3_rmsVoltage = 0;
    }
    float ADS3_amps = ADS3_rmsVoltage * COIL_CONVERSION_RATIO;
    float ADS3_watts = ADS3_amps * HOUSEHOLD_VOLTAGE;
    ADS3_energy += ADS3_watts;
    ADS3_energy_count++;

    struct tm timeinfo;
    getLocalTime(&timeinfo);
     

    if (millis() - PreviousDataDeliveryMS >= PeriodForDataDeliveryMS) {  // every 10 seconds.
      ADS1_totalWh += (ADS1_energy / NumberOfPeriodsInAnHour);
      ADS2_totalWh += (ADS2_energy / NumberOfPeriodsInAnHour);
      ADS3_totalWh += (ADS3_energy / NumberOfPeriodsInAnHour);

      if (timeinfo.tm_hour <= 4) {  // reset the total Wh accumulators at midnight and don't deliver to HA at night.
        ADS1_totalWh = 0;
        ADS2_totalWh = 0;
        ADS3_totalWh = 0;
      }
      else {
        float ADS1_avgWatts = ADS1_energy / ADS1_energy_count;
        float ADS2_avgWatts = ADS2_energy / ADS2_energy_count;
        float ADS3_avgWatts = ADS3_energy / ADS3_energy_count;
        sendToHomeAssistant(ADS1_avgWatts, ADS2_avgWatts, ADS3_avgWatts);
      }

      ADS1_energy = 0.0;
      ADS2_energy = 0.0;
      ADS3_energy = 0.0;
      ADS1_energy_count = 0;
      ADS2_energy_count = 0;
      ADS3_energy_count = 0;
      PreviousDataDeliveryMS += PeriodForDataDeliveryMS;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (timeinfo.tm_sec & 0b0100) {
      display.printf("mqtt: %s", mqtt_status);
    }
    else if (timeinfo.tm_sec & 0b1000) { //every 8 seconds
      display.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    }
    else {
      display.printf("WiFi %s %s", ssid, getSignalQuality());
    }
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.printf("Inv1:%4.0fW", ADS1_watts);
    display.setCursor(68, 16);
    //display.println(reading.ads1SampleCount);
    display.printf("%5.1fkWh", (ADS1_totalWh/1000));

    display.setCursor(0, 26);
    display.printf("Inv2:%4.0fW", ADS2_watts);
    display.setCursor(68, 26);
    //display.println(reading.ads2SampleCount);
    display.printf("%5.1fkWh", (ADS2_totalWh/1000));

    display.setCursor(0, 36);
    display.printf("Inv3:%4.0fW", ADS3_watts);
    display.setCursor(68, 36);
    //display.println(reading.ads3SampleCount);
    display.printf("%5.1fkWh", (ADS3_totalWh/1000));

    display.drawFastHLine(0, 47, 128, SSD1306_WHITE);

    display.setCursor(0, 50);
    display.printf("Tot:%5.0fW", (ADS1_watts+ADS2_watts+ADS3_watts));
    display.setCursor(68, 50);
    display.printf("%5.0fkWh", ((ADS3_totalWh+ADS2_totalWh+ADS1_totalWh)/1000));

    display.display();
}

TaskHandle_t UpdateDisplayHandle = NULL;
QueueHandle_t sensorQueue = xQueueCreate(100, sizeof(SensorReading));

void UpdateDisplay(void *pvParameters) {
  SensorReading reading;
  for (;;) {   
    if (xQueueReceive(sensorQueue, &reading, portMAX_DELAY)) {
      ItsBeenASecond(reading);
    }
    mqttClient.loop(); // this has to be called continuously or mqtt is unhappy
  }
}

void setup() {
  I2C_Sensors.begin(ADS_SDA_PIN,  ADS_SCL_PIN,  800000UL);
  I2C_Display.begin(OLED_SDA_PIN, OLED_SCL_PIN, 100000UL);
  delay(300);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }
  delay(500);
  splash();
  connectWiFi();
  setupTime();
  configureADS1();
  configureADS2();
  configureADS3();
  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
  mqttClient.setServer(mqtt_server, mqtt_port);
  delay(1500);
  
  xTaskCreatePinnedToCore(
    UpdateDisplay,       // task function
    "UpdateDisplay",     // name (for debugging)
    8192,              // stack size in bytes
    NULL,              // parameters to pass
    1,                 // priority (0 = lowest, higher = more urgent)
    &UpdateDisplayHandle, // handle
    0                  //  core 0
  );
  lastSampleMS = millis();
  PreviousDataDeliveryMS = lastSampleMS;
}

void loop() {
  if (millis() - lastSampleMS >= 1000) {  // every second.
    SensorReading reading = { 
      ADS1_sampleCount,
      ADS1_sumSquares,
      ADS2_sampleCount,
      ADS2_sumSquares,
      ADS3_sampleCount,
      ADS3_sumSquares
    };
    xQueueSend(sensorQueue, &reading, 0);

    ADS1_sumSquares = 0;
    ADS1_sampleCount = 0;
    ADS2_sumSquares = 0;
    ADS2_sampleCount = 0;
    ADS3_sumSquares = 0;
    ADS3_sampleCount = 0;
    lastSampleMS += 1000;
  }
  if (ADS1_Ready) {
    int16_t adc = ads1.getLastConversionResults();
    float voltage = ads1.computeVolts(adc);
    ADS1_sumSquares += voltage * voltage;
    ADS1_sampleCount++;
    ADS1_Ready = false;
  }
  if (ADS2_Ready) {
    int16_t adc = ads2.getLastConversionResults();
    float voltage = ads2.computeVolts(adc);
    ADS2_sumSquares += voltage * voltage;
    ADS2_sampleCount++;
    ADS2_Ready = false;
  }
  if (ADS3_Ready) {
    int16_t adc = ads3.getLastConversionResults();
    float voltage = ads3.computeVolts(adc);
    ADS3_sumSquares += voltage * voltage;
    ADS3_sampleCount++;
    ADS3_Ready = false;
  }
}
