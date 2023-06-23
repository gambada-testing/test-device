#include <Arduino.h>
#include <ESP32Time.h>

TaskHandle_t MainTask;
TaskHandle_t UpdateDataTask;
TaskHandle_t UpdateQRTask;

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

        vTaskDelay(pdMS_TO_TICKS(5000));
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

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    qrTaskStatus = STATUS_DEAD;
    Serial.println("DEAD QR CODE");
    vTaskDelete(UpdateQRTask);
}

void mainTask(void* parameter) {}

void setup() {
    Serial.begin(115200);
    Serial.println("Setup started.");

    pinMode(LED, OUTPUT);

    dataTaskStatus = STATUS_DEAD;
    qrTaskStatus = STATUS_DEAD;
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 1);

    print_wakeup_reason();

    xTaskCreatePinnedToCore(updateData, "updateDataTask", 10000, NULL, 10,
                            &UpdateDataTask, 0);

    xTaskCreatePinnedToCore(updateQR, "updateQRTask", 10000, NULL, 1,
                            &UpdateQRTask, 1);

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
