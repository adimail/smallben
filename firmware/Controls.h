#pragma once

#include "AudioEngine.h"
#include "BluetoothEngine.h"
#include "Config.h"
#include "RfidEngine.h"
#include "WifiManager.h"
#include <Arduino.h>

enum class PlaybackMode { Disc, Bluetooth };

enum class ButtonMachineState { Idle, DebouncePress, Held, DebounceRelease };

static PlaybackMode currentMode = PlaybackMode::Disc;

static int modeSwitchState = HIGH;
static int modeSwitchLastReading = HIGH;
static unsigned long modeSwitchDebounceTime = 0;
const unsigned long MODE_SWITCH_DEBOUNCE_MS = 50;

static int smoothedPotVal = -1;
static int lastAppliedPotVal = -1;
static unsigned long lastPotSampleTime = 0;
const unsigned long POT_SAMPLE_INTERVAL_MS = 50;

static ButtonMachineState btnState = ButtonMachineState::Idle;
static unsigned long btnStateChangeTime = 0;
static unsigned long btnPressStartTime = 0;
static bool btnLongPressFired = false;
const unsigned long BUTTON_DEBOUNCE_MS = 40;
const unsigned long LONG_PRESS_DURATION_MS = 800;

inline PlaybackMode getCurrentMode() { return currentMode; }

inline int readAveragedPot() {
	long sum = 0;
	for (int i = 0; i < 16; i++) {
		sum += analogRead(VOLUME_POT_PIN);
	}
	return (int)(sum / 16);
}

inline void applyPotVolume(int raw = -1) {
	if (currentMode != PlaybackMode::Disc) {
		return;
	}
	if (raw < 0) {
		raw = readAveragedPot();
	}
	if (raw < 40) {
		raw = 0;
	} else if (raw > 4055) {
		raw = 4095;
	}
	lastAppliedPotVal = raw;
	requestAudioVolume((float)raw / 4095.0f);
}

inline void handleVolume() {
	if (currentMode != PlaybackMode::Disc) {
		return;
	}
	unsigned long now = millis();
	if (now - lastPotSampleTime < POT_SAMPLE_INTERVAL_MS) {
		return;
	}
	lastPotSampleTime = now;

	int raw = readAveragedPot();
	if (smoothedPotVal < 0) {
		smoothedPotVal = raw;
	} else {
		smoothedPotVal = (smoothedPotVal * 7 + raw) / 8;
	}

	int candidate = smoothedPotVal;
	if (candidate < 40) {
		candidate = 0;
	} else if (candidate > 4055) {
		candidate = 4095;
	}

	if (abs(candidate - lastAppliedPotVal) > 60 || (candidate == 0 && lastAppliedPotVal != 0) ||
		(candidate == 4095 && lastAppliedPotVal != 4095)) {
		applyPotVolume(candidate);
	}
}

inline void applyMode(PlaybackMode mode) {
	heapLog("applyMode:enter");
	if (mode == PlaybackMode::Bluetooth) {
		disableWiFi();
		heapLog("applyMode:after disableWiFi");
		setRfidEnabled(false);
		requestAudioStop();
		requestAudioReleaseI2SBlocking();
		heapLog("applyMode:after releaseI2S");
		vTaskDelay(pdMS_TO_TICKS(100));
		if (!startBluetooth()) {
			requestAudioReclaimI2SBlocking();
			setRfidEnabled(true);
			currentMode = PlaybackMode::Disc;
			applyPotVolume();
			heapLog("applyMode:BT start FAILED, reverted to Disc");
			return;
		}
		currentMode = PlaybackMode::Bluetooth;
		heapLog("applyMode:now in Bluetooth");
	} else {
		stopBluetooth();
		heapLog("applyMode:after stopBluetooth");
		vTaskDelay(pdMS_TO_TICKS(50));
		requestAudioReclaimI2SBlocking();
		setRfidEnabled(true);
		currentMode = PlaybackMode::Disc;
		applyPotVolume();
		heapLog("applyMode:now in Disc");
	}
}

inline void handleModeSwitch() {
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

inline void onButtonShortPress() {
	if (currentMode == PlaybackMode::Disc) {
		requestAudioToggle();
	} else if (currentMode == PlaybackMode::Bluetooth) {
		toggleBluetoothPlayback();
	}
}

inline void onButtonLongPress() { toggleWiFi(); }

inline void handleButton() {
	int reading = digitalRead(WIFI_BUTTON_PIN);
	unsigned long now = millis();

	switch (btnState) {
	case ButtonMachineState::Idle:
		if (reading == LOW) {
			btnState = ButtonMachineState::DebouncePress;
			btnStateChangeTime = now;
		}
		break;

	case ButtonMachineState::DebouncePress:
		if (reading == LOW) {
			if (now - btnStateChangeTime >= BUTTON_DEBOUNCE_MS) {
				btnState = ButtonMachineState::Held;
				btnPressStartTime = now;
				btnLongPressFired = false;
			}
		} else {
			btnState = ButtonMachineState::Idle;
		}
		break;

	case ButtonMachineState::Held:
		if (reading == HIGH) {
			btnState = ButtonMachineState::DebounceRelease;
			btnStateChangeTime = now;
		} else {
			if (!btnLongPressFired && (now - btnPressStartTime >= LONG_PRESS_DURATION_MS)) {
				btnLongPressFired = true;
				onButtonLongPress();
			}
		}
		break;

	case ButtonMachineState::DebounceRelease:
		if (reading == HIGH) {
			if (now - btnStateChangeTime >= BUTTON_DEBOUNCE_MS) {
				btnState = ButtonMachineState::Idle;
				if (!btnLongPressFired) {
					onButtonShortPress();
				}
			}
		} else {
			btnState = ButtonMachineState::Held;
		}
		break;
	}
}

inline void initControls() {
	pinMode(VOLUME_POT_PIN, INPUT);
	pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
	pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);

	btnState = ButtonMachineState::Idle;
	btnLongPressFired = false;

	int initialModeReading = digitalRead(MODE_SWITCH_PIN);
	modeSwitchState = initialModeReading;
	modeSwitchLastReading = initialModeReading;
	if (initialModeReading == LOW) {
		applyMode(PlaybackMode::Bluetooth);
	} else {
		applyMode(PlaybackMode::Disc);
	}
}

inline void handleControls() {
	handleVolume();
	handleModeSwitch();
	handleButton();

	static bool lastBtConnected = false;
	if (currentMode == PlaybackMode::Bluetooth) {
		bool nowConnected = isBluetoothConnected();
		if (nowConnected != lastBtConnected) {
			lastBtConnected = nowConnected;
			heapLog(nowConnected ? "BT client CONNECTED" : "BT client DISCONNECTED");
		}
	}
}
