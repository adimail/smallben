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

	bool audioOk = initAudio();
	bool storageOk = initStorage();
	initServer();
	bool rfidOk = initRfid();
	initWiFi();

	if (audioOk) {
		startAudioTask();
	}
	if (rfidOk && storageOk) {
		startRfidTask();
	}

	delay(100);
	initControls();
}

void loop() {
	handleControls();

	if (isWiFiEnabled()) {
		handleServer();
	}
}
