#include <WiFi.h>
#include "AudioEngine.h"
#include "Config.h"
#include "RfidEngine.h"
#include "SmallbenServer.h"
#include "Storage.h"

void setup() {
	Serial.begin(115200);
	delay(200);

	WiFi.softAP(AP_SSID, AP_PASS);

	initAudio();
	initStorage();
	initServer();
	initRfid();

	startAudioTask();
	startRfidTask();
}

void loop() {
	handleServer();
}
