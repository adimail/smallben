#pragma once

#include "Config.h"
#include "SmallbenServer.h"
#include <Arduino.h>
#include <WiFi.h>

static bool wifiEnabled = false;
static bool wifiButtonState = HIGH;
static bool wifiLastReading = HIGH;
static unsigned long wifiLastDebounceTime = 0;

const unsigned long WIFI_BUTTON_DEBOUNCE_MS = 40;

inline void enableWiFi() {
	if (wifiEnabled || isBluetoothActive())
		return;

	WiFi.mode(WIFI_AP);

	if (!WiFi.softAP(AP_SSID, AP_PASS_DEFAULT, 1, 0, 2)) {
		WiFi.softAPdisconnect(true);
		WiFi.mode(WIFI_OFF);
		digitalWrite(WIFI_LED_PIN, LOW);
		wifiEnabled = false;
		return;
	}

	server.begin();

	wifiEnabled = true;
	digitalWrite(WIFI_LED_PIN, HIGH);
}

inline void disableWiFi() {
	if (!wifiEnabled)
		return;

	server.stop();
	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_OFF);

	wifiEnabled = false;
	digitalWrite(WIFI_LED_PIN, LOW);
}

inline void toggleWiFi() {
	if (wifiEnabled)
		disableWiFi();
	else
		enableWiFi();
}

inline bool isWiFiEnabled() { return wifiEnabled; }

inline void initWiFiControl() {
	pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
	pinMode(WIFI_LED_PIN, OUTPUT);

	digitalWrite(WIFI_LED_PIN, LOW);

	wifiButtonState = digitalRead(WIFI_BUTTON_PIN);
	wifiLastReading = wifiButtonState;
}

inline void handleWiFiButton() {
	bool reading = digitalRead(WIFI_BUTTON_PIN);

	if (reading != wifiLastReading)
		wifiLastDebounceTime = millis();

	if ((millis() - wifiLastDebounceTime) > WIFI_BUTTON_DEBOUNCE_MS) {
		if (reading != wifiButtonState) {
			wifiButtonState = reading;

			if (wifiButtonState == LOW)
				toggleWiFi();
		}
	}

	wifiLastReading = reading;
}
