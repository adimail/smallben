#include "AudioEngine.h"
#include "Config.h"
#include "RfidEngine.h"
#include "SmallbenServer.h"
#include <WiFi.h>

void setup() {
	Serial.begin(115200);
	delay(200);

	WiFi.softAP(AP_SSID, AP_PASS);
	Serial.print("[WiFi] AP started. Connect to: ");
	Serial.println(AP_SSID);
	Serial.print("[WiFi] Dashboard URL: http://");
	Serial.println(WiFi.softAPIP());

	initAudio();
	initServer();
	initRfid();
	startAudioTask();
	startRfidTask();
}

void loop() {
	handleServer();
}
