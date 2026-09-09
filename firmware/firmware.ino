#include <SPI.h>
#include <SD.h>

#define SD_CS 5

void printDirectory(File dir, int indent) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    for (int i = 0; i < indent; i++) Serial.print("  ");
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, indent + 1);
    } else {
      Serial.print("\t\t");
      Serial.print(entry.size());
      Serial.println(" bytes");
    }
    entry.close();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card init failed!");
    return;
  }
  Serial.println("SD card initialized.");

  File root = SD.open("/");
  printDirectory(root, 0);
  root.close();
}

void loop() {
  // nothing here yet
}
