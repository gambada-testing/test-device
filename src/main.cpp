#include <Arduino.h>
#include <Wire.h>
#include <stdbool.h>
#include <stdint.h>

#include "DisplayDriver.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare_42.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "qrcodegen.h"

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

void Clear_Screen() {
  gfx.fillBuffer(EPD_WHITE);
  gfx.commit();
  delay(2000);
}

int vref = 1100;

uint32_t timeStamp = 0;

void setup() {
  gfx.init();
  gfx.setRotation(1);
  gfx.fillBuffer(EPD_WHITE);

  Serial.begin(115200);  // Set console baud rate
}

void loop() {
  bool ok = qrcodegen_encodeText(
      "0a12345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "012345567890123456789012345678901234567890123456789012345678901234567890"
      "0123",
      tempBuffer, qr0, qrcodegen_Ecc_MEDIUM, 32, 32, qrcodegen_Mask_AUTO, true);

  int pixel = 2;
  int offset = 5;

  int size = qrcodegen_getSize(qr0);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      //(... paint qrcodegen_getModule(qr0, x, y)...)
      if (qrcodegen_getModule(qr0, x, y)) {
        //Serial.print("*");
        gfx.setColor(EPD_BLACK);
        gfx.fillRect(x * pixel + offset, y * pixel + offset, pixel, pixel);
      } else {
        //Serial.print(" ");
        gfx.setColor(EPD_WHITE);
        gfx.fillRect(x * pixel + offset, y * pixel + offset, pixel, pixel);
      }
    }
    //Serial.print("\n");
  }

  gfx.commit();
  delay(100000);  // Delay before we do it again
  Clear_Screen();
}
