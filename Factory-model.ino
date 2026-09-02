
 #include <WiFi.h>
 #include <PubSubClient.h>
 #include <Wire.h>
 #include <INA226_WE.h>
 #include <Adafruit_SHT31.h>
 #include <OneWire.h>
 #include <DallasTemperature.h>
 #include <ESP32Servo.h>
 #include <ArduinoJson.h>
 #include <Update.h>
 #include <HTTPClient.h>
 #include <esp_task_wdt.h>
 #include <esp_ota_ops.h>
 #include <Preferences.h>

 // ============================================================================
 // CONFIGURATION
 // ============================================================================
 namespace WifiConfig {
   constexpr char SSID[]      = "OOMNI-EYE-2.4GHz";
   constexpr char PASSWORD[]  = "Admin@2026";
 }

 namespace ThingsBoardConfig {
   constexpr char SERVER[]    = "allcad-chennai.selfip.com";
   constexpr uint16_t PORT    = 1883;
   constexpr char TOKEN[]     = "C5cOYHn2LGzMBLgjnm6l";
   constexpr char CLIENT_ID[] = "FactoryModel";

   // --- OTA Configuration ---
   constexpr int OTA_PORT       = 8081;
   constexpr char FW_TITLE[]    = "FactoryModelFirmware";
   constexpr char FW_VERSION[]  = "1.0.1"; // Bump this to trigger an update

   // --- LWT (Last Will and Testament) ---
   constexpr char LWT_TOPIC[]   = "v1/devices/me/telemetry";
   constexpr char LWT_MESSAGE[] = "{\"status\":\"offline\"}";
   constexpr int  LWT_QOS       = 1;
   constexpr bool LWT_RETAIN    = false;
 }

 // OTA stall watchdog: abort transfer if no bytes received within this window
 constexpr unsigned long OTA_STALL_TIMEOUT_MS = 15000;

 namespace NvsConfig {
   constexpr char NAMESPACE[]      = "factory";
   constexpr char KEY_TOTAL_COUNT[] = "totalCnt";
 }

 // Product-count persistence: batch NVS writes to limit flash wear.
 // Flushed early (forced) before any reboot/OTA restart so nothing is lost.
 constexpr uint8_t COUNT_SAVE_EVERY_N          = 5;
 constexpr unsigned long COUNT_SAVE_INTERVAL_MS = 30000;

 WiFiClient wifiClient;
 PubSubClient mqtt(wifiClient);
 Preferences nvs;

 // ============================================================================
 // HARDWARE PIN MAP
 // ============================================================================
 namespace Pins {
   constexpr uint8_t SDA_1 = 21;
   constexpr uint8_t SCL_1 = 22;
   constexpr uint8_t SDA_2 = 26;
   constexpr uint8_t SCL_2 = 25;

   constexpr uint8_t SWITCHYARD_TEMP = 4;
   constexpr uint8_t E18_D80NK       = 13;

   constexpr uint8_t LASER_SW1 = 27;
   constexpr uint8_t LASER_SW2 = 32;
   constexpr uint8_t LASER_SW3 = 33;
   constexpr uint8_t LASER_SW4 = 34;

   constexpr uint8_t MOTOR_ENA       = 19;
   constexpr uint8_t MIST1           = 14;
   constexpr uint8_t MIST2           = 23;
   constexpr uint8_t HEATER_RELAY    = 18;
   constexpr uint8_t LIGHTING_MOSFET = 12;

   constexpr uint8_t PERSON1_SERVO = 16;
   constexpr uint8_t PERSON2_SERVO = 17;

   constexpr uint8_t RED_LED   = 5;
   constexpr uint8_t GREEN_LED = 2;
   constexpr uint8_t BUZZER    = 15;
 }

 constexpr uint8_t INA226_ADDR = 0x45;
 constexpr uint8_t SHT31_ADDR  = 0x44;

 // ============================================================================
 // PERIPHERAL INSTANCES
 // ============================================================================
 TwoWire I2C_Zone1 = TwoWire(0);
 TwoWire I2C_Zone2 = TwoWire(1);

 INA226_WE lightingIna(&I2C_Zone1, INA226_ADDR);
 INA226_WE conveyorIna(&I2C_Zone2, INA226_ADDR);

 Adafruit_SHT31 coolingTower1Sht(&I2C_Zone1);
 Adafruit_SHT31 coolingTower2Sht(&I2C_Zone2);

 OneWire oneWire(Pins::SWITCHYARD_TEMP);
 DallasTemperature switchyardTempSensor(&oneWire);

 // ============================================================================
 // SYSTEM STATE & CACHE
 // ============================================================================
 struct SensorHealth {
   bool lightingIna = false;
   bool conveyorIna = false;
   bool coolingTower1 = false;
   bool coolingTower2 = false;
   bool switchyardTemp = false;
 } sensorOK;

 struct SensorReadings {
   float lightingVoltage = 0, lightingCurrent = 0, lightingPower = 0;
   float conveyorVoltage = 0, conveyorCurrent = 0, conveyorPower = 0;
   float coolingTower1Temp = 0, coolingTower1Hum = 0;
   float coolingTower2Temp = 0, coolingTower2Hum = 0;
   float switchyardTemp = 0;
 } readings;

 struct SystemState {
   bool conveyorOn      = false;
   bool mist1On         = false;
   bool mist2On         = false;
   bool heaterOn        = false;
   bool lightingOn      = false;
   bool redLedOn        = false;
   bool greenLedOn      = false;
   bool intrusion       = false;
   unsigned long productCount      = 0; // current run only, resets on reboot
   unsigned long productCountTotal = 0; // lifetime total, persisted to NVS
 } state;

 // Product-count NVS write batching state
 uint8_t pendingCountWrites = 0;
 unsigned long lastCountSaveAt = 0;

 // ============================================================================
 // UTILITY CLASSES
 // ============================================================================
 class SmoothServo {
   public:
     void attach(uint8_t pin, int minUs = 500, int maxUs = 2400) {
       _servo.setPeriodHertz(50);
       _servo.attach(pin, minUs, maxUs);
     }
     void begin(int startAngle) {
       _current = constrain(startAngle, 0, 180);
       _target  = _current;
       _servo.write(_current);
     }
     void moveTo(int targetAngle) {
       _target = constrain(targetAngle, 0, 180);
     }
     void update() {
       if (_current == _target) return;
       if (millis() - _lastStepAt < _stepDelayMs) return;
       _lastStepAt = millis();
       _current += (_target > _current) ? 1 : -1;
       _servo.write(_current);
     }
     int position() const { return _current; }
     bool isMoving() const  { return _current != _target; }
   private:
     Servo _servo;
     int _current = 0;
     int _target  = 0;
     unsigned long _lastStepAt  = 0;
     unsigned long _stepDelayMs = 15;
 };

 SmoothServo person1Servo;
 SmoothServo person2Servo;

 // ============================================================================
 // TIMING GLOBALS
 // ============================================================================
 constexpr unsigned long SENSOR_SAMPLE_INTERVAL_MS = 2000;
 unsigned long lastSensorSample = 0;
 unsigned long lastWifiAttempt = 0;
 unsigned long lastMqttAttempt = 0;

 bool otaInProgress = false;
 bool otaCheckedThisSession = false;

 // ============================================================================
 // FORWARD DECLARATIONS
 // ============================================================================
 void initGpio();
 void initServos();
 void initI2C();
 void initSensors();
 void initWifi();
 void initMqtt();

 void maintainConnection();
 void mqttCallback(char *topic, byte *payload, unsigned int length);
 void handleRpc(const String &method, JsonVariant params, int requestId);
 void requestFirmwareAttributes();
 void performOtaUpdate(const String &fwTitle, const String &fwVersion, const String &fwChecksum, const String &fwChecksumAlgo, long fwSize);
 void reportFirmwareState(const char* fwState, const char* error = nullptr);
 void reportCurrentFirmwareInfo();
 void publishOnlineStatus();

 void sampleSensors();
 void monitorProductCount();
 void loadPersistedProductCount();
 void savePersistedProductCount(bool force = false);
 void resetProductCount();
 void logSensorData();
 void publishTelemetry();
 void checkLaserFence();
 bool laserBlocked(uint8_t pin);

 void setConveyor(bool on);
 void setMist1(bool on);
 void setMist2(bool on);
 void setHeater(bool on);
 void setLighting(bool on);
 void setPerson1Angle(int angle);
 void setPerson2Angle(int angle);
 void togglePerson1();
 void togglePerson2();
 void homePerson1();
 void homePerson2();

 void buildStateJson(JsonDocument &doc);
 void handleSerialCommand(char c);

 // ============================================================================
 // MAIN SETUP
 // ============================================================================
 void setup() {
   Serial.begin(115200);
   delay(300);

   Serial.println(F("\n=================================================="));
   Serial.println(F("       FACTORY MODEL - COMPLETE SYSTEM"));
   Serial.println(F("       (Professional Cloud + OTA Edition)"));
   Serial.printf("       Firmware Version: %s\n", ThingsBoardConfig::FW_VERSION);
   Serial.println(F("=================================================="));

   esp_task_wdt_config_t twdt_config = {
     .timeout_ms = 10000,
     .idle_core_mask = (1 << 0),
     .trigger_panic = true
   };
   esp_err_t wdtInitResult = esp_task_wdt_init(&twdt_config);
   if (wdtInitResult == ESP_ERR_INVALID_STATE) {
       esp_task_wdt_reconfigure(&twdt_config);
   }
   esp_task_wdt_add(NULL);

   initGpio();
   initServos();
   initI2C();
   initSensors();
   loadPersistedProductCount();
   initWifi();
   initMqtt();

   Serial.println(F("System ready. Connecting to Network..."));
 }

 // ============================================================================
 // MAIN LOOP
 // ============================================================================
 void loop() {
   esp_task_wdt_reset();

   maintainConnection();
   mqtt.loop();

   person1Servo.update();
   person2Servo.update();

   monitorProductCount();
   savePersistedProductCount(); // cheap no-op unless a batch/interval threshold is met
   checkLaserFence();

   if (Serial.available()) {
     handleSerialCommand(Serial.read());
   }

   if (millis() - lastSensorSample >= SENSOR_SAMPLE_INTERVAL_MS) {
     lastSensorSample = millis();
     sampleSensors();
     logSensorData();
     publishTelemetry();
   }
 }

 // ============================================================================
 // INITIALIZATION
 // ============================================================================
 void initGpio() {
   pinMode(Pins::E18_D80NK, INPUT_PULLUP);
   pinMode(Pins::LASER_SW1, INPUT);
   pinMode(Pins::LASER_SW2, INPUT);
   pinMode(Pins::LASER_SW3, INPUT);
   pinMode(Pins::LASER_SW4, INPUT);
   pinMode(Pins::MOTOR_ENA, OUTPUT);
   pinMode(Pins::MIST1, OUTPUT);
   pinMode(Pins::MIST2, OUTPUT);
   pinMode(Pins::HEATER_RELAY, OUTPUT);
   pinMode(Pins::LIGHTING_MOSFET, OUTPUT);
   pinMode(Pins::RED_LED, OUTPUT);
   pinMode(Pins::GREEN_LED, OUTPUT);

   // Configure PWM tone channel for Pin 15 (Frequency 2000Hz, 8-bit resolution)
   ledcAttach(Pins::BUZZER, 2000, 8);
   ledcWriteTone(Pins::BUZZER, 0); // Start silent

   digitalWrite(Pins::MOTOR_ENA, LOW);
   digitalWrite(Pins::MIST1, LOW);
   digitalWrite(Pins::MIST2, LOW);
   digitalWrite(Pins::HEATER_RELAY, HIGH);
   digitalWrite(Pins::LIGHTING_MOSFET, LOW);
   digitalWrite(Pins::RED_LED, HIGH);
   digitalWrite(Pins::GREEN_LED, LOW);
 }

 void initServos() {
   ESP32PWM::allocateTimer(1);
   ESP32PWM::allocateTimer(2);
   person1Servo.attach(Pins::PERSON1_SERVO, 500, 2400);
   person2Servo.attach(Pins::PERSON2_SERVO, 500, 2400);

   person1Servo.begin(0);
   person2Servo.begin(180);
 }

 void initI2C() {
   I2C_Zone1.begin(Pins::SDA_1, Pins::SCL_1, 100000);
   I2C_Zone2.begin(Pins::SDA_2, Pins::SCL_2, 100000);
 }

 void initSensors() {
   sensorOK.lightingIna = lightingIna.init();
   if (sensorOK.lightingIna) lightingIna.setResistorRange(0.1, 1.0);

   sensorOK.coolingTower1 = coolingTower1Sht.begin(SHT31_ADDR);

   sensorOK.conveyorIna = conveyorIna.init();
   if (sensorOK.conveyorIna) conveyorIna.setResistorRange(0.1, 1.0);

   sensorOK.coolingTower2 = coolingTower2Sht.begin(SHT31_ADDR);

   switchyardTempSensor.begin();
   switchyardTempSensor.setWaitForConversion(false);
   sensorOK.switchyardTemp = (switchyardTempSensor.getDeviceCount() > 0);
   if (sensorOK.switchyardTemp) switchyardTempSensor.requestTemperatures();

   // Surface any failed sensor inits immediately in the serial log so a bad
   // I2C wire or address doesn't just look like a "stuck value" later.
   if (!sensorOK.lightingIna)    Serial.println(F("[WARN] Lighting INA226 not detected."));
   if (!sensorOK.conveyorIna)    Serial.println(F("[WARN] Conveyor INA226 not detected."));
   if (!sensorOK.coolingTower1)  Serial.println(F("[WARN] Cooling Tower 1 SHT31 not detected."));
   if (!sensorOK.coolingTower2)  Serial.println(F("[WARN] Cooling Tower 2 SHT31 not detected."));
   if (!sensorOK.switchyardTemp) Serial.println(F("[WARN] Switchyard DS18B20 not detected."));
 }

 void initWifi() {
   WiFi.mode(WIFI_STA);
   WiFi.begin(WifiConfig::SSID, WifiConfig::PASSWORD);
 }

 void initMqtt() {
   mqtt.setServer(ThingsBoardConfig::SERVER, ThingsBoardConfig::PORT);
   mqtt.setCallback(mqttCallback);
   mqtt.setBufferSize(1024);
   mqtt.setSocketTimeout(8);
 }

 // ============================================================================
 // SENSOR LOGIC
 // ============================================================================
 void monitorProductCount() {
   static unsigned long lastDebounceTime = 0;
   static bool lastRawState = false;
   static bool steadyState = false;

   bool rawState = (digitalRead(Pins::E18_D80NK) == LOW);

   if (rawState != lastRawState) {
     lastDebounceTime = millis();
   }

   if ((millis() - lastDebounceTime) > 25) {
     if (rawState != steadyState) {
       steadyState = rawState;
       if (steadyState == true && state.conveyorOn) {
         state.productCount++;
         state.productCountTotal++;
         pendingCountWrites++;
         savePersistedProductCount(); // writes only if batch/interval threshold met
         Serial.print(F("\n[+] Product detected! Run: "));
         Serial.print(state.productCount);
         Serial.print(F(" | Lifetime Total: "));
         Serial.println(state.productCountTotal);
       }
     }
   }
   lastRawState = rawState;
 }

 // ============================================================================
 // PRODUCT COUNT PERSISTENCE (NVS)
 // ============================================================================
 void loadPersistedProductCount() {
   nvs.begin(NvsConfig::NAMESPACE, false);
   state.productCountTotal = nvs.getULong(NvsConfig::KEY_TOTAL_COUNT, 0);
   lastCountSaveAt = millis();
   Serial.print(F("[NVS] Loaded lifetime product count: "));
   Serial.println(state.productCountTotal);
 }

 void savePersistedProductCount(bool force) {
   bool countThreshold = pendingCountWrites >= COUNT_SAVE_EVERY_N;
   bool intervalHit     = (millis() - lastCountSaveAt) >= COUNT_SAVE_INTERVAL_MS && pendingCountWrites > 0;

   if (!force && !countThreshold && !intervalHit) return;

   nvs.putULong(NvsConfig::KEY_TOTAL_COUNT, state.productCountTotal);
   pendingCountWrites = 0;
   lastCountSaveAt = millis();
 }

 void resetProductCount() {
   state.productCount = 0;
   state.productCountTotal = 0;
   pendingCountWrites = 0;
   savePersistedProductCount(true); // force immediate flush of the reset value
   Serial.println(F("[NVS] Product count reset (run + lifetime total)."));
 }

 void sampleSensors() {
   if (sensorOK.lightingIna) {
     readings.lightingVoltage = lightingIna.getBusVoltage_V();
     readings.lightingCurrent = lightingIna.getCurrent_mA();
     readings.lightingPower   = readings.lightingVoltage * (readings.lightingCurrent / 1000.0f);
   }

   if (sensorOK.conveyorIna) {
     readings.conveyorVoltage = conveyorIna.getBusVoltage_V();
     readings.conveyorCurrent = conveyorIna.getCurrent_mA();
     readings.conveyorPower   = readings.conveyorVoltage * (readings.conveyorCurrent / 1000.0f);
   }

   if (sensorOK.coolingTower1) {
     float t = coolingTower1Sht.readTemperature();
     float h = coolingTower1Sht.readHumidity();
     if (!isnan(t)) readings.coolingTower1Temp = t;
     if (!isnan(h)) readings.coolingTower1Hum  = h;
   }

   if (sensorOK.coolingTower2) {
     float t = coolingTower2Sht.readTemperature();
     float h = coolingTower2Sht.readHumidity();
     if (!isnan(t)) readings.coolingTower2Temp = t;
     if (!isnan(h)) readings.coolingTower2Hum  = h;
   }

   if (sensorOK.switchyardTemp) {
     float t = switchyardTempSensor.getTempCByIndex(0);
     if (t != DEVICE_DISCONNECTED_C) readings.switchyardTemp = t;
     switchyardTempSensor.requestTemperatures();
   }
 }

 bool laserBlocked(uint8_t pin) { return digitalRead(pin) == HIGH; }

 void checkLaserFence() {
   state.intrusion = (laserBlocked(Pins::LASER_SW1) ||
                      laserBlocked(Pins::LASER_SW2) ||
                      laserBlocked(Pins::LASER_SW3) ||
                      laserBlocked(Pins::LASER_SW4));

   // High-volume pulsing alarm pattern using PWM tone modulation
   if (state.intrusion) {
       static unsigned long lastToneToggle = 0;
       static bool highTone = false;
       if (millis() - lastToneToggle >= 200) {
           lastToneToggle = millis();
           highTone = !highTone;
           ledcWriteTone(Pins::BUZZER, highTone ? 2500 : 1500); // Alternates between 1.5kHz and 2.5kHz
       }
   } else {
       ledcWriteTone(Pins::BUZZER, 0); // Completely silent when safe
   }
 }

 // ============================================================================
 // ACTUATOR CONTROLS
 // ============================================================================
 void setConveyor(bool on) {
   state.conveyorOn = on;
   digitalWrite(Pins::MOTOR_ENA, on ? HIGH : LOW);
   digitalWrite(Pins::GREEN_LED, on ? HIGH : LOW);
   digitalWrite(Pins::RED_LED,   on ? LOW : HIGH);
   state.greenLedOn = on;
   state.redLedOn   = !on;
   Serial.println(on ? F("Conveyor Status : Running") : F("Conveyor Status : Stopped"));
 }

 void setMist1(bool on)   { state.mist1On = on; digitalWrite(Pins::MIST1, on ? HIGH : LOW); }
 void setMist2(bool on)   { state.mist2On = on; digitalWrite(Pins::MIST2, on ? HIGH : LOW); }
 void setHeater(bool on)  { state.heaterOn = on; digitalWrite(Pins::HEATER_RELAY, on ? LOW : HIGH); }
 void setLighting(bool on){ state.lightingOn = on; digitalWrite(Pins::LIGHTING_MOSFET, on ? HIGH : LOW); }

 void setPerson1Angle(int angle) { person1Servo.moveTo(angle); }
 void setPerson2Angle(int angle) { person2Servo.moveTo(angle); }

 void togglePerson1() {
   if (person1Servo.position() == 0) setPerson1Angle(180);
   else setPerson1Angle(0);
 }

 void togglePerson2() {
   if (person2Servo.position() == 180) setPerson2Angle(0);
   else setPerson2Angle(180);
 }

 void homePerson1() {
   person1Servo.moveTo(0);
   Serial.println(F("Person 1 returned to Home position (0°)"));
 }

 void homePerson2() {
   person2Servo.moveTo(180);
   Serial.println(F("Person 2 returned to Home position (180°)"));
 }

 // ============================================================================
 // NETWORK / CLOUD
 // ============================================================================
 void maintainConnection() {
   if (WiFi.status() != WL_CONNECTED) {
     if (millis() - lastWifiAttempt >= 5000) {
       lastWifiAttempt = millis();
       WiFi.disconnect();
       WiFi.begin(WifiConfig::SSID, WifiConfig::PASSWORD);
     }
     return;
   }

   if (!mqtt.connected()) {
     if (millis() - lastMqttAttempt >= 5000) {
       lastMqttAttempt = millis();
       esp_task_wdt_reset();

       // Connect with Last Will & Testament: if the device drops off
       // ungracefully, the broker publishes this on our behalf so
       // ThingsBoard reflects "offline" without waiting on a stale timeout.
       bool connected = mqtt.connect(
           ThingsBoardConfig::CLIENT_ID,
           ThingsBoardConfig::TOKEN,
           nullptr,
           ThingsBoardConfig::LWT_TOPIC,
           ThingsBoardConfig::LWT_QOS,
           ThingsBoardConfig::LWT_RETAIN,
           ThingsBoardConfig::LWT_MESSAGE
       );

       if (connected) {
         mqtt.subscribe("v1/devices/me/rpc/request/+");
         mqtt.subscribe("v1/devices/me/attributes");
         mqtt.subscribe("v1/devices/me/attributes/response/+");

         static bool rollbackChecked = false;
         if (!rollbackChecked) {
             rollbackChecked = true;
             const esp_partition_t *running = esp_ota_get_running_partition();
             esp_ota_img_states_t otaState;
             if (esp_ota_get_state_partition(running, &otaState) == ESP_OK && otaState == ESP_OTA_IMG_PENDING_VERIFY) {
                 esp_ota_mark_app_valid_cancel_rollback();
                 Serial.println(F("[OTA] New firmware confirmed working - rollback cancelled."));
             }
         }

         if (!otaCheckedThisSession) {
             otaCheckedThisSession = true;
             requestFirmwareAttributes();
         }

         publishOnlineStatus();
         reportCurrentFirmwareInfo(); // announce running fw_title/fw_version as client attributes
         publishTelemetry();
       }
       esp_task_wdt_reset();
     }
   }
 }

 void publishOnlineStatus() {
   if (!mqtt.connected()) return;
   mqtt.publish(ThingsBoardConfig::LWT_TOPIC, "{\"status\":\"online\"}");
 }

 void buildStateJson(JsonDocument &doc) {
   doc["productCount"] = state.productCount;           // current run only
   doc["productCountTotal"] = state.productCountTotal; // lifetime, persisted
   doc["laserSwitchyard1"] = laserBlocked(Pins::LASER_SW1) ? "Intrusion" : "Safe";
   doc["laserSwitchyard2"] = laserBlocked(Pins::LASER_SW2) ? "Intrusion" : "Safe";
   doc["laserSwitchyard3"] = laserBlocked(Pins::LASER_SW3) ? "Intrusion" : "Safe";
   doc["laserSwitchyard4"] = laserBlocked(Pins::LASER_SW4) ? "Intrusion" : "Safe";
   doc["lightingPower"] = readings.lightingPower;
   doc["conveyorPower"] = readings.conveyorPower;
   doc["coolingTower1Temp"] = readings.coolingTower1Temp;
   doc["coolingTower1Hum"]  = readings.coolingTower1Hum;
   doc["coolingTower2Temp"] = readings.coolingTower2Temp;
   doc["coolingTower2Hum"]  = readings.coolingTower2Hum;
   doc["switchyardTemp"] = readings.switchyardTemp;
   doc["motor"] = state.conveyorOn;
   doc["conveyorStatus"] = state.conveyorOn ? "Conveyor running" : "Conveyor stop";
   doc["mist1"] = state.mist1On;
   doc["mist2"] = state.mist2On;
   doc["heater"] = state.heaterOn;
   doc["lighting"] = state.lightingOn;
   doc["person1Servo"] = person1Servo.position();
   doc["person2Servo"] = person2Servo.position();
   doc["intrusion"] = state.intrusion;

   // --- Sensor health flags (new) ---
   // Lets the ThingsBoard dashboard distinguish "sensor genuinely failed at
   // boot" from "value just hasn't changed" or "device offline."
   doc["lightingIna_ok"]    = sensorOK.lightingIna;
   doc["conveyorIna_ok"]    = sensorOK.conveyorIna;
   doc["coolingTower1_ok"]  = sensorOK.coolingTower1;
   doc["coolingTower2_ok"]  = sensorOK.coolingTower2;
   doc["switchyardTemp_ok"] = sensorOK.switchyardTemp;
 }

 void publishTelemetry() {
   if (!mqtt.connected()) return;
   JsonDocument doc;
   buildStateJson(doc);
   String payload;
   serializeJson(doc, payload);
   mqtt.publish("v1/devices/me/telemetry", payload.c_str());
 }

 // ============================================================================
 // ADVANCED OTA HANDLERS (ThingsBoard HTTP API + MD5)
 // ============================================================================
 void reportFirmwareState(const char* fwState, const char* error) {
   JsonDocument doc;
   doc["fw_state"] = fwState;
   if (error != nullptr) doc["fw_error"] = error;

   // Bounded buffer: serializeJson(doc, buf, size) truncates safely instead
   // of overflowing if `error` (e.g. Update.errorString()) is long.
   char payload[256];
   size_t written = serializeJson(doc, payload, sizeof(payload));
   (void)written;
   // Published as a CLIENT-SIDE ATTRIBUTE (not telemetry) - this is the key
   // ThingsBoard expects for its built-in OTA progress tracking/widgets.
   mqtt.publish("v1/devices/me/attributes", payload);
   Serial.printf("[OTA] State: %s%s%s\n", fwState, error ? " - " : "", error ? error : "");
 }

 void reportCurrentFirmwareInfo() {
   if (!mqtt.connected()) return;
   JsonDocument doc;
   doc["fw_title"]   = ThingsBoardConfig::FW_TITLE;
   doc["fw_version"] = ThingsBoardConfig::FW_VERSION;
   char payload[128];
   serializeJson(doc, payload, sizeof(payload));
   // Client-side attribute: this is what a dashboard widget reads to show
   // "currently running firmware," and it changes automatically the moment
   // a newly-flashed device reconnects with a different FW_VERSION.
   mqtt.publish("v1/devices/me/attributes", payload);
 }

 void requestFirmwareAttributes() {
   JsonDocument reqDoc;
   reqDoc["sharedKeys"] = "fw_title,fw_version,fw_checksum,fw_checksum_algorithm,fw_size";
   char payload[128];
   serializeJson(reqDoc, payload, sizeof(payload));
   mqtt.publish("v1/devices/me/attributes/request/1", payload);
   Serial.println(F("[OTA] Requested firmware shared attributes."));
 }

 void performOtaUpdate(const String &fwTitle, const String &fwVersion,
                       const String &fwChecksum, const String &fwChecksumAlgo,
                       long fwSize) {
   if (otaInProgress) return;
   otaInProgress = true;

   Serial.printf("[OTA] Update available: %s (current: %s)\n", fwVersion.c_str(), ThingsBoardConfig::FW_VERSION);
   reportFirmwareState("DOWNLOADING");

   if (fwChecksumAlgo != "MD5") {
       reportFirmwareState("FAILED", "Unsupported checksum algorithm (expected MD5)");
       otaInProgress = false;
       return;
   }

   String url = "http://" + String(ThingsBoardConfig::SERVER) + ":" + String(ThingsBoardConfig::OTA_PORT) +
                "/api/v1/" + String(ThingsBoardConfig::TOKEN) + "/firmware?title=" + fwTitle +
                "&version=" + fwVersion;

   WiFiClient otaClient;
   HTTPClient http;

   http.setConnectTimeout(10000);
   http.setTimeout(10000);
   http.begin(otaClient, url);

   esp_task_wdt_reset();
   int httpCode = http.GET();
   esp_task_wdt_reset();

   if (httpCode != HTTP_CODE_OK) {
       Serial.printf("[OTA] HTTP GET failed, code: %d\n", httpCode);
       reportFirmwareState("FAILED", "HTTP download request failed");
       http.end();
       otaInProgress = false;
       return;
   }

   int contentLen = http.getSize();
   if (contentLen <= 0 || (fwSize > 0 && contentLen != fwSize)) {
       reportFirmwareState("FAILED", "Content length mismatch");
       http.end();
       otaInProgress = false;
       return;
   }

   if (!Update.begin(contentLen)) {
       reportFirmwareState("FAILED", "Insufficient OTA partition space");
       http.end();
       otaInProgress = false;
       return;
   }

   Update.setMD5(fwChecksum.c_str());

   WiFiClient *stream = http.getStreamPtr();
   uint8_t buf[512];
   int written = 0;
   unsigned long lastProgressLog = millis();
   unsigned long lastByteReceivedAt = millis(); // stall watchdog reference

   while (http.connected() && written < contentLen) {
       size_t avail = stream->available();
       if (avail) {
           int toRead = avail > sizeof(buf) ? sizeof(buf) : avail;
           int n = stream->readBytes(buf, toRead);
           if (n > 0) {
               lastByteReceivedAt = millis(); // reset stall timer on progress
               if (Update.write(buf, n) != (size_t)n) {
                   reportFirmwareState("FAILED", "Flash write error");
                   Update.abort();
                   http.end();
                   otaInProgress = false;
                   return;
               }
               written += n;
           }
       }

       // Stalled-transfer watchdog: http.connected() can stay true even if
       // the server has stopped sending data. Without this, the loop below
       // (kept alive by esp_task_wdt_reset()) would spin forever.
       if (millis() - lastByteReceivedAt > OTA_STALL_TIMEOUT_MS) {
           reportFirmwareState("FAILED", "Transfer stalled - no data received");
           Update.abort();
           http.end();
           otaInProgress = false;
           return;
       }

       esp_task_wdt_reset();

       if (mqtt.connected()) {
           mqtt.loop();
       }

       if (millis() - lastProgressLog > 3000) {
           lastProgressLog = millis();
           Serial.printf("[OTA] Progress: %d / %d bytes\n", written, contentLen);
       }
       delay(1);
   }
   http.end();

   if (written != contentLen) {
       reportFirmwareState("FAILED", "Download incomplete");
       Update.abort();
       otaInProgress = false;
       return;
   }

   reportFirmwareState("VERIFIED");
   reportFirmwareState("UPDATING");

   if (!Update.end(true)) {
       reportFirmwareState("FAILED", Update.errorString());
       otaInProgress = false;
       return;
   }

   reportFirmwareState("UPDATED");
   Serial.println(F("[OTA] Update successful. Rebooting..."));
   savePersistedProductCount(true); // flush any pending count before restart
   delay(1000);
   esp_restart();
 }

 void mqttCallback(char *topic, byte *payload, unsigned int length) {
   String topicStr(topic);
   String msg;
   for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

   if (topicStr.startsWith("v1/devices/me/attributes")) {
     JsonDocument attrDoc;
     if (deserializeJson(attrDoc, msg)) return;

     JsonVariant attrs;
     if (attrDoc.containsKey("shared")) {
         attrs = attrDoc["shared"];
     } else {
         attrs = attrDoc.as<JsonVariant>();
     }

     if (attrs.containsKey("fw_title") && attrs.containsKey("fw_version") &&
         attrs.containsKey("fw_checksum") && attrs.containsKey("fw_checksum_algorithm")) {

         String fwTitle    = attrs["fw_title"].as<String>();
         String fwVersion  = attrs["fw_version"].as<String>();
         String fwChecksum = attrs["fw_checksum"].as<String>();
         String fwAlgo     = attrs["fw_checksum_algorithm"].as<String>();
         long fwSize       = attrs.containsKey("fw_size") ? attrs["fw_size"].as<long>() : 0;

         if (fwTitle == ThingsBoardConfig::FW_TITLE && fwVersion != String(ThingsBoardConfig::FW_VERSION)) {
             performOtaUpdate(fwTitle, fwVersion, fwChecksum, fwAlgo, fwSize);
         }
     }
     return;
   }

   int requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1).toInt();
   JsonDocument rpcDoc;
   if (!deserializeJson(rpcDoc, msg)) {
       handleRpc(rpcDoc["method"] | "", rpcDoc["params"], requestId);
   }
 }

 void handleRpc(const String &method, JsonVariant params, int requestId) {
   if      (method == "setConveyor")                           setConveyor(params.as<bool>());
   else if (method == "setMist1")                              setMist1(params.as<bool>());
   else if (method == "setMist2")                              setMist2(params.as<bool>());
   else if (method == "setHeater")                             setHeater(params.as<bool>());
   else if (method == "setLighting")                           setLighting(params.as<bool>());
   else if (method == "setPerson1" || method == "setServo1")   setPerson1Angle(params.as<int>());
   else if (method == "setPerson2" || method == "setServo2")   setPerson2Angle(params.as<int>());
   else if (method == "homePerson1" || method == "homeServo1") homePerson1();
   else if (method == "homePerson2" || method == "homeServo2") homePerson2();
   else if (method == "resetProductCount")                     resetProductCount();
   else if (method == "reboot") {
       Serial.println(F("[SYS] Remote reboot triggered via RPC!"));
       savePersistedProductCount(true); // flush any pending count before restart
       delay(1000);
       esp_restart();
   }

   if (requestId >= 0 && mqtt.connected()) {
     JsonDocument stateDoc;
     buildStateJson(stateDoc);
     String response;
     serializeJson(stateDoc, response);
     mqtt.publish(("v1/devices/me/rpc/response/" + String(requestId)).c_str(), response.c_str());
   }
   publishTelemetry();
 }

 // ============================================================================
 // LOGGING & DEBUG
 // ============================================================================
 void logSensorData() {
   Serial.println(F("--------------------------------------------------"));
   Serial.print(F("Conveyor Status     : ")); Serial.println(state.conveyorOn ? F("Conveyor running") : F("Conveyor stop"));
   Serial.print(F("Product Count       : ")); Serial.println(state.productCount);
   Serial.print(F("Switchyard Temp     : ")); Serial.print(readings.switchyardTemp, 2); Serial.println(F(" C"));
   Serial.print(F("Laser Switchyard 1  : ")); Serial.println(laserBlocked(Pins::LASER_SW1) ? F("Intrusion") : F("Safe"));
   Serial.print(F("Laser Switchyard 2  : ")); Serial.println(laserBlocked(Pins::LASER_SW2) ? F("Intrusion") : F("Safe"));
   Serial.print(F("Laser Switchyard 3  : ")); Serial.println(laserBlocked(Pins::LASER_SW3) ? F("Intrusion") : F("Safe"));
   Serial.print(F("Laser Switchyard 4  : ")); Serial.println(laserBlocked(Pins::LASER_SW4) ? F("Intrusion") : F("Safe"));
   Serial.print(F("Lighting Power      : ")); Serial.print(readings.lightingPower, 3); Serial.println(F(" W"));
   Serial.print(F("Conveyor Power      : ")); Serial.print(readings.conveyorPower, 3); Serial.println(F(" W"));
   Serial.print(F("Cooling Tower 1     : ")); Serial.print(readings.coolingTower1Temp, 2); Serial.print(F(" C, ")); Serial.print(readings.coolingTower1Hum, 2); Serial.println(F(" %RH"));
   Serial.print(F("Cooling Tower 2     : ")); Serial.print(readings.coolingTower2Temp, 2); Serial.print(F(" C, ")); Serial.print(readings.coolingTower2Hum, 2); Serial.println(F(" %RH"));
   Serial.println(F("--------------------------------------------------"));
 }

 void handleSerialCommand(char command) {
   switch (command) {
     case 'm': setConveyor(!state.conveyorOn); break;
     case '1': setMist1(!state.mist1On);       break;
     case '2': setMist2(!state.mist2On);       break;
     case 'h': setHeater(!state.heaterOn);     break;
     case 'l': setLighting(!state.lightingOn); break;
     case 'p': togglePerson1();                break;
     case 'o': togglePerson2();                break;
     case 't': sampleSensors(); logSensorData(); publishTelemetry(); break;
     default: break;
   }
 }
