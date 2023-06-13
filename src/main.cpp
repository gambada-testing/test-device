#include <Arduino.h>
#include <SPI.h>  // Built-in
#include <Wire.h>
#include <esp_adc_cal.h>

#include "DisplayDriver.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare_42.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "qrcode.h"  // Copyright (c) //https://github.com/ricmoo/qrcode/

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

QRCode qrcode;

// #########################################################################################
void Display_QRcode(int offset_x, int offset_y, int element_size, int QRsize,
                    int ECC_Mode, const char *Message) {
  // QRcode capacity examples Size-12  65 x 65 LOW      883 535 367
  //                                           MEDIUM   691 419 287
  //                                           QUARTILE 489 296 203
  //                                           HIGH     374 227 155
  uint8_t qrcodeData[qrcode_getBufferSize(QRsize)];
  // ECC_LOW, ECC_MEDIUM, ECC_QUARTILE and ECC_HIGH. Higher levels of error
  // correction sacrifice data capacity, but ensure damaged codes remain
  // readable.
  if (ECC_Mode % 4 == 0)
    qrcode_initText(&qrcode, qrcodeData, QRsize, ECC_LOW, Message);
  if (ECC_Mode % 4 == 1)
    qrcode_initText(&qrcode, qrcodeData, QRsize, ECC_MEDIUM, Message);
  if (ECC_Mode % 4 == 2)
    qrcode_initText(&qrcode, qrcodeData, QRsize, ECC_QUARTILE, Message);
  if (ECC_Mode % 4 == 3)
    qrcode_initText(&qrcode, qrcodeData, QRsize, ECC_HIGH, Message);
  for (int y = 0; y < qrcode.size; y++) {
    for (int x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        gfx.setColor(EPD_BLACK);
        gfx.fillRect(x * element_size + offset_x, y * element_size + offset_y,
                     element_size, element_size);
      } else {
        gfx.setColor(EPD_WHITE);
        gfx.fillRect(x * element_size + offset_x, y * element_size + offset_y,
                     element_size, element_size);
      }
    }
  }
}

// #########################################################################################
void Clear_Screen() {
  gfx.fillBuffer(EPD_WHITE);
  gfx.commit();
  delay(2000);
}

int vref = 1100;
uint32_t timeStamp = 0;

// #########################################################################################
void setup() {
  gfx.init();
  gfx.setRotation(1);
  gfx.fillBuffer(EPD_WHITE);

  Serial.begin(115200);  // Set console baud rate
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100,
      &adc_chars);  // Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    vref = adc_chars.vref;
  }
}

// #########################################################################################
void loop() {
  // Display_QRcode(x,y,element_size,QRcode Size,Error Detection/Correction
  // Level,"string for encoding");
  gfx.drawString(50, 10, "San Vitale");

  uint16_t v = analogRead(ADC_PIN);
  float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  String voltage = "Voltage :" + String(battery_voltage) + "V\n";



  // When connecting USB, the battery detection will return 0,
  // because the adc detection circuit is disconnected when connecting USB
  Serial.println(voltage);
  gfx.drawString(160, 10, voltage);
  if (voltage == "0.00") {
    Serial.println("USB is connected, please disconnect USB.");
  }

  gfx.drawString(20, 40, "Temperatura: 40°C | Umidità: troppa");
  gfx.drawString(5, 70, "Qulità dell'aria: pessima | Pressione: alta");

  Display_QRcode(30, 130, 2, 26, 1,
                 "0000001111111122222222223344555556600000011111111222222222233"
                 "4455555660000001111111122222222223344555556600000011111111222"
                 "22222223344555556600000011111111222222222233445555566");
  gfx.commit();
  delay(10000);  // Delay before we do it again
  Clear_Screen();
}
