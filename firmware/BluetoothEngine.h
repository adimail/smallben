#pragma once
#include "Config.h"
#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include <ESP_I2S.h>

static I2SClass i2s;
static BluetoothA2DPSink a2dp_sink(i2s);
static bool btRunning = false;

inline bool initBluetooth() {
	i2s.setPins(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
	return i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
					 I2S_STD_SLOT_BOTH);
}

inline bool startBluetooth() {
	if (btRunning)
		return true;
	if (!initBluetooth()) {
		return false;
	}
	a2dp_sink.start(BT_DEVICE_NAME);
	btRunning = true;
	return true;
}

inline void stopBluetooth() {
	if (!btRunning)
		return;
	a2dp_sink.end(false);
	i2s.end();
	btRunning = false;
}

inline bool isBluetoothActive() { return btRunning; }

inline bool isBluetoothConnected() { return btRunning && a2dp_sink.is_connected(); }

inline void setBluetoothVolume(uint8_t volume) {
	if (btRunning) {
		a2dp_sink.set_volume(volume);
	}
}

inline void toggleBluetoothPlayback() {
	if (!btRunning || !a2dp_sink.is_connected()) {
		return;
	}
	if (a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
		a2dp_sink.pause();
	} else {
		a2dp_sink.play();
	}
}
