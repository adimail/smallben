#include "AudioEngine.h"
#include "BluetoothEngine.h"
#include "Config.h"
#include "Controls.h"
#include "RfidEngine.h"
#include "SmallbenServer.h"
#include "Storage.h"
#include "WifiManager.h"

static TaskHandle_t appTaskHandle = nullptr;

void appTaskLoop(void *param) {
	unsigned long lastLog = 0;
	for (;;) {
		handleControls();
		if (isWiFiEnabled()) {
			handleServer();
		}
		if (millis() - lastLog > 3000) {
			lastLog = millis();
			heapLog("loop tick");
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

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

	xTaskCreatePinnedToCore(appTaskLoop, "appTask", 4096, nullptr, 1, &appTaskHandle, 0);
}

void loop() {
	vTaskDelay(pdMS_TO_TICKS(1000));
}
