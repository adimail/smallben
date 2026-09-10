#include "AudioEngine.h"
#include "Config.h"
#include "RfidEngine.h"
#include "SmallbenServer.h"
#include "Storage.h"
#include "WifiManager.h"

void setup() {
	Serial.begin(115200);
	delay(200);

	initAudio();
	initStorage();
	initServer();
	initRfid();

	initWiFiControl();

	startAudioTask();
	startRfidTask();
}

void loop() {
  handleWiFiButton();

  if (isWiFiEnabled())
		handleServer();
}
