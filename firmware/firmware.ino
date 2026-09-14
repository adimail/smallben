#include "AudioEngine.h"
#include "BluetoothEngine.h"
#include "Config.h"
#include "Controls.h"
#include "RfidEngine.h"
#include "SmallbenServer.h"
#include "Storage.h"
#include "WifiManager.h"

void setup() {
	Serial.begin(115200);
	delay(200);
	heapLog("boot");

	bool audioOk = initAudio();
	heapLog("after initAudio");

	initStorage();
	heapLog("after initStorage");

	initServer();
	heapLog("after initServer");

	initRfid();
	heapLog("after initRfid");

	initWiFi();
	heapLog("after initWiFi(pin setup only)");

	if (audioOk) {
		startAudioTask();
		heapLog("after startAudioTask");
	}

	delay(100);

	if (audioOk) {
		requestAudioPlayOnce("/smallben/plen.mp3");
	}

	initControls();
	heapLog("after initControls");
}

void loop() {
	static unsigned long lastLog = 0;

	handleControls();
	handleRfidLoop();

	if (isWiFiEnabled()) {
		handleServer();
	}

	if (millis() - lastLog > 3000) {
		lastLog = millis();
		heapLog("loop tick");
	}

	vTaskDelay(pdMS_TO_TICKS(1));
}
