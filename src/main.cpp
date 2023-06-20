
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

void listAllFiles() {
  // List all available files (if any) in the SPI Flash File System
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file) {
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
  if(file) {
    // Deserialize the JSON data
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, file);
    double data_str = doc["data_out"];
    Serial.println(data_str);
  }
  file.close();  
}

void writeDataToFile(const char* filename) {
  File outfile = SPIFFS.open(filename,"w");
  StaticJsonDocument<200> doc;
  doc["data_out"] = 3.14159;
  if(serializeJson(doc, outfile)==0) {
    Serial.println("Failed to write to SPIFFS file");
  } else {
    Serial.println("Success!");
  }
  outfile.close();  
}

void setup() {
  Serial.begin(115200);
  // Initialize the SPIFFS object
  if(!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    while(true){}
  }

  const char* filename = "/data2.json";
  writeDataToFile(filename);
  listAllFiles();  
  readDataFromFile(filename);
}

void loop() {}
