#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP32Time.h>
#include <SPIFFS.h>

#include "DisplayDriver.h"
#include "EPD_WaveShare.h"
#include "EPD_WaveShare_42.h"
#include "MiniGrafx.h"
#include "qrcodegen.h"

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

uint32_t timeStamp = 0;

#define LED 12

#define uS_TO_S_FACTOR \
    1000000              /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 10 /* Time ESP32 will go to sleep (in seconds) */

#define STATUS_DEAD 0
#define STATUS_ALIVE 1

static volatile uint8_t dataTaskStatus = STATUS_DEAD;
static volatile uint8_t qrTaskStatus = STATUS_DEAD;

ESP32Time rtc;

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

            delay(100);
            esp_deep_sleep_start();
            break;
    }
}

void updateData(void* parameter) {
    Serial.println("BORN ENV DATA");
    if (esp_sleep_get_wakeup_cause() ==
        ESP_SLEEP_WAKEUP_TIMER) {  // oppure se è sveglio ma son passati 60
                                   // minuti allora:

        dataTaskStatus = STATUS_ALIVE;
        /*Serial.print("DEAD ENV  core: ");
        Serial.print(xPortGetCoreID());*/
        Serial.println();
        Serial.println("ENV DATA UPDATE");

        Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
        Serial.println(rtc.getEpoch());

        //vTaskDelay(pdMS_TO_TICKS(1000));
    }

    dataTaskStatus = STATUS_DEAD;
    Serial.println("DEAD ENV DATA");
    // xTaskNotifyGive(MainTask);
    vTaskDelete(UpdateDataTask);
}

void updateQR(void* parameter) {
    Serial.println("BORN QR CODE");
    if (esp_sleep_get_wakeup_cause() ==
        ESP_SLEEP_WAKEUP_EXT0) {  // oppure se è sveglio e viene premuto il
                                  // pulsante
        qrTaskStatus = STATUS_ALIVE;
        Serial.print("QR CODE core: ");
        Serial.print(xPortGetCoreID());
        Serial.println();
        Serial.println("QR CODE UPDATE: " + qrTaskStatus);

        gfx.drawString(5, 10, rtc.getDateTime(false));

        uint16_t v = analogRead(ADC_PIN);
        float battery_voltage =
            ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
        String voltage = "Voltage:" + String(battery_voltage) + "V";
        gfx.drawString(195, 10, voltage);

        gfx.drawString(20, 40, "Temperatura: 40°C | Umidità: troppa");
        gfx.drawString(5, 70, "Qulità dell'aria: pessima | Pressione: alta");

        bool ok = qrcodegen_encodeText(
            "012345678901234567890123456789012345678901234567890123456789",
            tempBuffer, qr0, qrcodegen_Ecc_MEDIUM, 32, 32, qrcodegen_Mask_AUTO,
            true);

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
        Serial.println("Commit display");

        gfx.commit();

        vTaskDelay(pdMS_TO_TICKS(5000));  // wait 1 minute

        // print sensor data
        gfx.commit();
    }
    qrTaskStatus = STATUS_DEAD;
    Serial.println("DEAD QR CODE");
    vTaskDelete(UpdateQRTask);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Setup started.");

    gfx.init();
    gfx.setRotation(1);
    gfx.fillBuffer(EPD_WHITE);

    pinMode(LED, OUTPUT);

    dataTaskStatus = STATUS_DEAD;
    qrTaskStatus = STATUS_DEAD;
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 1);

    print_wakeup_reason();

    xTaskCreatePinnedToCore(updateData, "updateDataTask", 3000, NULL, 2,
                            &UpdateDataTask, 0);

    xTaskCreatePinnedToCore(updateQR, "updateQRTask", 3000, NULL, 1,
                            &UpdateQRTask, 0);

    // Serial.println("data: " + dataTaskStatus);
    // Serial.println("qr: " + qrTaskStatus);

    // Attesa che i task muoiano
    delay(100);
    while (qrTaskStatus == 1 || dataTaskStatus == 1) {
    }

    // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    delay(100);
    Serial.println("Sleep");
    Serial.flush();

    esp_deep_sleep_start();
}

void loop() {}
