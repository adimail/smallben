#include <WiFi.h>
#include "Config.h"
#include "SmallbenServer.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  // Start Wi-Fi Access Point
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[WiFi] AP started. Connect to: ");
  Serial.println(AP_SSID);
  Serial.print("[WiFi] Dashboard URL: http://");
  Serial.println(WiFi.softAPIP());

  // Initialize SD card & Web Server routes
  initServer();
}

void loop() {
  handleServer();
}

