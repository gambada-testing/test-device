#include <Arduino.h>
#include <Wire.h>
#include <stdbool.h>
#include <stdint.h>

#include "qrcodegen.h"

// Text data
uint8_t qr0[qrcodegen_BUFFER_LEN_MAX];
uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

void setup() {
  Serial.begin(115200);
  bool ok = qrcodegen_encodeText(
      "Hello, world!", tempBuffer, qr0, qrcodegen_Ecc_MEDIUM,
      32, 32, qrcodegen_Mask_AUTO, true);

  int size = qrcodegen_getSize(qr0);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      //(... paint qrcodegen_getModule(qr0, x, y)...)
      if (qrcodegen_getModule(qr0, x, y)) {
        Serial.print("*");
      } else {
        Serial.print(" ");
      }
    }
    Serial.print("\n");
  }
}

void loop() {}
