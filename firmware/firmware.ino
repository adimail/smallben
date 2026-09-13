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

	bool storageOk = initStorage();
	heapLog("after initStorage");

	initServer();
	heapLog("after initServer");

	bool rfidOk = initRfid();
	heapLog("after initRfid");

	initWiFi();
	heapLog("after initWiFi(pin setup only)");

	if (audioOk) {
		startAudioTask();
		heapLog("after startAudioTask");
	}
	if (rfidOk && storageOk) {
		startRfidTask();
		heapLog("after startRfidTask");
	}

	delay(100);
	initControls();
	heapLog("after initControls");
}

void loop() {
	handleControls();
	if (isWiFiEnabled()) {
		handleServer();
	}
	static unsigned long lastLog = 0;
	if (millis() - lastLog > 3000) {
		lastLog = millis();
		heapLog("loop tick");
	}
}
