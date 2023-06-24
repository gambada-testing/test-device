#include "Arduino.h"
#include "ArduinoJson.h"
#include "DisplayDriver.h"
#include "EPD_WaveShare.h"
#include "EPD_WaveShare_42.h"
#include "ESP32Time.h"
#include "MiniGrafx.h"
#include "SPIFFS.h"
#include "bsec.h"
#include "qrcodegen.h"

const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

TaskHandle_t UpdateDataTask;
TaskHandle_t UpdateQRTask;

// Text data
uint8_t qr0[qrcodegen_BUFFER_LEN_MAX];
uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

#define SCREEN_WIDTH 400.0
#define SCREEN_HEIGHT 300.0
#define BITS_PER_PIXEL 1
#define EPD_BLACK 0
#define EPD_WHITE 1
uint16_t palette[] = {0, 1};

#define ADC_PIN 35

// pins_arduino.h, e.g. LOLIN32 D322
static const uint8_t EPD_BUSY = 34;
static const uint8_t EPD_SS = 5;
static const uint8_t EPD_RST = 33;
static const uint8_t EPD_DC = 32;
static const uint8_t EPD_SCK = 18;
// static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no
// data from display
static const uint8_t EPD_MOSI = 23;

EPD_WaveShare42 epd(EPD_SS, EPD_RST, EPD_DC, EPD_BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

int vref = 1100;

int64_t espTime = 0;
RTC_DATA_ATTR int64_t previousEspTime = 0;

#define LED 12

#define uS_TO_S_FACTOR \
    1000000              /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 10 /* Time ESP32 will go to sleep (in seconds) */

#define STATUS_DEAD 0
#define STATUS_ALIVE 1

static volatile uint8_t dataTaskStatus = STATUS_DEAD;
static volatile uint8_t qrTaskStatus = STATUS_DEAD;

static volatile uint8_t updateDataStatus = STATUS_DEAD;

ESP32Time rtc;
Bsec sensor;

SemaphoreHandle_t xMutex;

RTC_DATA_ATTR uint8_t sensor_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};
RTC_DATA_ATTR int64_t sensor_state_time = 0;

bsec_virtual_sensor_t sensor_list[] = {
    BSEC_OUTPUT_RAW_TEMPERATURE, BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY, BSEC_OUTPUT_IAQ};

void listAllFiles() {
    // List all available files (if any) in the SPI Flash File System
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.print("FILE: ");
        Serial.println(file.name());
        file = root.openNextFile();
    }
    root.close();
    file.close();
}

void readDataFromFile(const char* filename) {
    // Read JSON data from a file
    File file = SPIFFS.open(filename);
    if (file) {
        // Deserialize the JSON data
        StaticJsonDocument<2048> doc;
        DeserializationError error = deserializeJson(doc, file);
        double data_str = doc["data_out"];
        Serial.println(data_str);
    }
    file.close();
}

void initDataToFile(const char* filename) {
    File file = SPIFFS.open(filename, FILE_WRITE);
    DynamicJsonDocument doc(10000);

    doc["id"] = "34234234234234234234465";
    doc["version"] = 1.2;

    JsonObject coordinates = doc.createNestedObject("coordinates");
    coordinates["lat"] = 44.3234;
    coordinates["lon"] = 44.2344;

    JsonArray env_data = doc.createNestedArray("env_data");

    JsonObject elem = env_data.createNestedObject();
    elem["timestamp"] = rtc.getEpoch();
    elem["temperature"] = sensor.rawTemperature;
    elem["humidity"] = sensor.rawHumidity;
    elem["pressure"] = sensor.pressure;
    elem["air_quality"] = sensor.iaq;

    // serializeJsonPretty(doc, Serial);
    if (serializeJsonPretty(doc, file) == 0) {
        Serial.println("Failed to write to SPIFFS file");
    } else {
        Serial.println("File Success!");
    }
    file.close();
}

void addDataToFile(const char* filename) {
    // Read JSON data from a file
    File file = SPIFFS.open(filename, FILE_READ);
    DynamicJsonDocument doc(10000);

    if (file) {
        // Deserialize the JSON data
        DeserializationError error = deserializeJson(doc, file);
    }
    file.close();

    file = SPIFFS.open(filename, FILE_WRITE);
    JsonArray array = doc["env_data"];
    JsonObject elem = array.createNestedObject();

    elem["timestamp"] = rtc.getEpoch();
    elem["temperature"] = sensor.rawTemperature;
    elem["humidity"] = sensor.rawHumidity;
    elem["pressure"] = sensor.pressure;
    elem["air_quality"] = sensor.iaq;

    /*serializeJsonPretty(array, Serial);
    Serial.println("");*/
    // serializeJsonPretty(doc, Serial);

    if (serializeJsonPretty(doc, file) == 0) {
        Serial.println("Failed to write to SPIFFS file");
    } else {
        Serial.println("Success!");
    }
    file.close();
}

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println("Wakeup caused by ULP program");
            break;
        default:
            Serial.printf("Wakeup was not caused by deep sleep: %d\n",
                          wakeup_reason);

            // init all

            rtc.setTime(00, 19, 11, 23, 6, 2023);

            if (sensor.run()) {
                const char* filename = "/env_data.json";
                initDataToFile(filename);
            }
            previousEspTime = esp_timer_get_time();

            delay(100);
            esp_deep_sleep_start();
            break;
    }
}

void updateData(void* parameter) {
    Serial.println("- BORN ENV DATA");
    dataTaskStatus = STATUS_ALIVE;

    while (1) {
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER ||
            updateDataStatus == 1) {
            Serial.println("- ENV DATA UPDATE");

            xSemaphoreTake(xMutex, portMAX_DELAY);
            Serial.println("- ENV DATA SEMAPHORE TAKE");
            Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
            Serial.println(rtc.getEpoch());

            if (sensor.run()) {
                const char* filename = "/env_data.json";
                addDataToFile(filename);
            }

            Serial.println("- ENV DATA SEMAPHORE GIVE");
            xSemaphoreGive(xMutex);

            vTaskDelay(pdMS_TO_TICKS(2000));

            Serial.println("- DEAD ENV DATA inside");
            dataTaskStatus = STATUS_DEAD;
            vTaskDelete(UpdateDataTask);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        if (qrTaskStatus == STATUS_DEAD) {
            Serial.println("- DEAD ENV DATA outside");
            dataTaskStatus = STATUS_DEAD;
            vTaskDelete(UpdateDataTask);
        }
    }
}

void updateQR(void* parameter) {
    Serial.println("- BORN QR CODE");
    qrTaskStatus = STATUS_ALIVE;
    while (1) {
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 ||
            digitalRead(GPIO_NUM_27) == HIGH) {
            Serial.print("- QR CODE core: ");
            Serial.print(xPortGetCoreID());
            Serial.println();
            Serial.println("- QR CODE UPDATE: " + qrTaskStatus);

            xSemaphoreTake(xMutex, portMAX_DELAY);
            Serial.println("- QR CODE SEMAPHORE TAKE");

            gfx.drawString(5, 10, rtc.getDateTime(false));

            uint16_t v = analogRead(ADC_PIN);
            float battery_voltage =
                ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
            String voltage = "Voltage:" + String(battery_voltage) + "V";
            gfx.drawString(195, 10, voltage);

            // vTaskDelay(pdMS_TO_TICKS(1000));  // wait 1 minute

            // if (sensor.run()) {
            sensor.run();
            gfx.drawString(5, 40,
                           "Temperature: " + String(sensor.rawTemperature) +
                               "°C | Humidity: " + String(sensor.rawHumidity) +
                               "%");

            gfx.drawString(5, 70,
                           "Air quality: " + String(sensor.iaq) +
                               " | Pressure:" + String(sensor.pressure / 100) +
                               " hPa");
            //}

            bool ok = qrcodegen_encodeText(
                "012345678901234567890123456789012345678901234567890123456789",
                tempBuffer, qr0, qrcodegen_Ecc_MEDIUM, 32, 32,
                qrcodegen_Mask_AUTO, true);

            int pixel = 2;
            int offsetX = 5;
            int offsetY = 105;

            int size = qrcodegen_getSize(qr0);
            for (int y = 0; y < size; y++) {
                for (int x = 0; x < size; x++) {
                    //(... paint qrcodegen_getModule(qr0, x, y)...)
                    if (qrcodegen_getModule(qr0, x, y)) {
                        // Serial.print("*");
                        gfx.setColor(EPD_BLACK);
                        gfx.fillRect(x * pixel + offsetX, y * pixel + offsetY,
                                     pixel, pixel);
                    } else {
                        // Serial.print(" ");
                        gfx.setColor(EPD_WHITE);
                        gfx.fillRect(x * pixel + offsetX, y * pixel + offsetY,
                                     pixel, pixel);
                    }
                }
            }
            // Serial.println("Commit display");

            gfx.commit();

            // vTaskDelay(pdMS_TO_TICKS(5000));  // wait 1 minute
            // gfx.commit();
            Serial.println("- QR CODE SEMAPHORE GIVE");

            xSemaphoreGive(xMutex);

            qrTaskStatus = STATUS_DEAD;
            Serial.println("- DEAD QR CODE inside");
            vTaskDelete(UpdateQRTask);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        if (dataTaskStatus == STATUS_DEAD) {
            Serial.println("- DEAD QR CODE outside");
            qrTaskStatus = STATUS_DEAD;
            vTaskDelete(UpdateQRTask);
        }
        // delay(20);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("--- AWAKE ---");

    xMutex = xSemaphoreCreateMutex();

    SPIFFS.begin(true);

    gfx.init();
    gfx.setRotation(1);
    gfx.fillBuffer(EPD_WHITE);

    sensor.begin(0x77, Wire);
    sensor.setConfig(bsec_config_iaq);
    sensor.updateSubscription(sensor_list,
                              sizeof(sensor_list) / sizeof(sensor_list[0]),
                              BSEC_SAMPLE_RATE_LP);

    pinMode(LED, OUTPUT);
    pinMode(GPIO_NUM_27, INPUT);

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 1);

    dataTaskStatus = STATUS_DEAD;
    qrTaskStatus = STATUS_DEAD;

    updateDataStatus = 0;

    print_wakeup_reason();

    xTaskCreatePinnedToCore(updateData, "updateDataTask", 10000, NULL, 10,
                            &UpdateDataTask, 0);

    xTaskCreatePinnedToCore(updateQR, "updateQRTask", 10000, NULL, 9,
                            &UpdateQRTask, 0);

    // Serial.println("data: " + dataTaskStatus);
    // Serial.println("qr: " + qrTaskStatus);

    // Attesa che i task nascano
    delay(500);

    // Attesa che i task muoiano
    while (qrTaskStatus == STATUS_ALIVE || dataTaskStatus == STATUS_ALIVE) {
        espTime = esp_timer_get_time();

        if (qrTaskStatus == STATUS_ALIVE &&
            ((espTime - previousEspTime) >
             (TIME_TO_SLEEP * uS_TO_S_FACTOR) - (1 * uS_TO_S_FACTOR)) &&
            ((espTime - previousEspTime) <
             ((TIME_TO_SLEEP * uS_TO_S_FACTOR) + (1 * uS_TO_S_FACTOR)))) {
            updateDataStatus = 1;
            Serial.println("@ UPDATE DATA STATUS");
        }
    }

    // update wake time
    if ((espTime - previousEspTime) < (TIME_TO_SLEEP * uS_TO_S_FACTOR) &&
        updateDataStatus == 0) {
        uint64_t tempo =
            (TIME_TO_SLEEP * uS_TO_S_FACTOR) - (espTime - previousEspTime);

        esp_sleep_enable_timer_wakeup(tempo);

        Serial.println("* UPDATE WAKE TIME TO: " + String(tempo/uS_TO_S_FACTOR));
    }

    delay(100);
    Serial.println("--- SLEEP ---");
    Serial.flush();

    previousEspTime = esp_timer_get_time();

    esp_deep_sleep_start();
}

void loop() { vTaskDelete(NULL); }
