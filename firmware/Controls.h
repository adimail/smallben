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

static int lastPotVal = -100;
static unsigned long lastPotSampleTime = 0;
const unsigned long POT_SAMPLE_INTERVAL_MS = 50;

static ButtonMachineState btnState = ButtonMachineState::Idle;
static unsigned long btnStateChangeTime = 0;
static unsigned long btnPressStartTime = 0;
static bool btnLongPressFired = false;
const unsigned long BUTTON_DEBOUNCE_MS = 40;
const unsigned long LONG_PRESS_DURATION_MS = 1500;

inline PlaybackMode getCurrentMode() { return currentMode; }

inline void applyPotVolume() {
	int raw = analogRead(VOLUME_POT_PIN);
	lastPotVal = raw;
	if (currentMode == PlaybackMode::Bluetooth) {
		setBluetoothVolume(raw / 32);
	} else {
		requestAudioVolume((float)raw / 4095.0f);
	}
}

inline void handleVolume() {
	unsigned long now = millis();
	if (now - lastPotSampleTime < POT_SAMPLE_INTERVAL_MS) {
		return;
	}
	lastPotSampleTime = now;

	int raw = analogRead(VOLUME_POT_PIN);
	if (abs(raw - lastPotVal) > 30) {
		applyPotVolume();
	}
}

inline void applyMode(PlaybackMode mode) {
	if (mode == PlaybackMode::Bluetooth) {
		disableWiFi();
		setRfidEnabled(false);
		requestAudioStop();
		requestAudioReleaseI2SBlocking();
		vTaskDelay(pdMS_TO_TICKS(100));
		if (!startBluetooth()) {
			requestAudioReclaimI2SBlocking();
			setRfidEnabled(true);
			currentMode = PlaybackMode::Disc;
			applyPotVolume();
			return;
		}
		currentMode = PlaybackMode::Bluetooth;
	} else {
		stopBluetooth();
		vTaskDelay(pdMS_TO_TICKS(50));
		requestAudioReclaimI2SBlocking();
		setRfidEnabled(true);
		currentMode = PlaybackMode::Disc;
	}
	applyPotVolume();
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
}
