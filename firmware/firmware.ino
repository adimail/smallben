#include "AudioEngine.h"
#include "BluetoothEngine.h"
#include "Config.h"
#include "RfidEngine.h"
#include "SmallbenServer.h"
#include "Storage.h"
#include "WifiManager.h"

enum class PlaybackMode { Disc, Bluetooth };

static PlaybackMode currentMode = PlaybackMode::Disc;
static int modeSwitchState = HIGH;
static int modeSwitchLastReading = HIGH;
static unsigned long modeSwitchDebounceTime = 0;
const unsigned long MODE_SWITCH_DEBOUNCE_MS = 50;

void applyMode(PlaybackMode mode) {
	if (mode == PlaybackMode::Bluetooth) {
		setRfidEnabled(false);
		requestAudioStop();
		releaseAudioI2S();
		startBluetooth();
		currentMode = PlaybackMode::Bluetooth;
	} else {
		stopBluetooth();
		reclaimAudioI2S();
		setRfidEnabled(true);
		currentMode = PlaybackMode::Disc;
	}
}

void initModeSwitch() {
	pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
	int initialReading = digitalRead(MODE_SWITCH_PIN);
	modeSwitchState = initialReading;
	modeSwitchLastReading = initialReading;
	if (initialReading == LOW) {
		applyMode(PlaybackMode::Bluetooth);
	} else {
		applyMode(PlaybackMode::Disc);
	}
}

void handleModeSwitch() {
	int reading = digitalRead(MODE_SWITCH_PIN);
	if (reading != modeSwitchLastReading) {
		modeSwitchDebounceTime = millis();
	}
	if ((millis() - modeSwitchDebounceTime) > MODE_SWITCH_DEBOUNCE_MS) {
		if (reading != modeSwitchState) {
			modeSwitchState = reading;
			if (modeSwitchState == LOW) {
				applyMode(PlaybackMode::Bluetooth);
			} else {
				applyMode(PlaybackMode::Disc);
			}
		}
	}
	modeSwitchLastReading = reading;
}

void setup() {
	Serial.begin(115200);
	delay(200);

	initAudio();
	initStorage();
	initServer();
	initRfid();

	initWiFiControl();
	initModeSwitch();

	startAudioTask();
	startRfidTask();
}

void loop() {
	handleWiFiButton();
	handleModeSwitch();

	if (isWiFiEnabled()) {
		handleServer();
	}
}
