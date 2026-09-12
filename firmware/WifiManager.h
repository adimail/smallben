#pragma once

#include "Config.h"
#include "SmallbenServer.h"
#include <Arduino.h>
#include <WiFi.h>

enum class WifiStatus { Success, BlockedByBluetooth, ApStartFailed, Disabled };

static bool wifiEnabled = false;

inline void updateWifiLed() { digitalWrite(WIFI_LED_PIN, wifiEnabled ? HIGH : LOW); }

inline WifiStatus enableWiFi() {
	if (wifiEnabled) {
		updateWifiLed();
		return WifiStatus::Success;
	}

	extern bool isBluetoothActive();
	if (isBluetoothActive()) {
		wifiEnabled = false;
		updateWifiLed();
		return WifiStatus::BlockedByBluetooth;
	}

	vTaskDelay(pdMS_TO_TICKS(20));

	WiFi.mode(WIFI_AP);

	if (!WiFi.softAP(AP_SSID, AP_PASS_DEFAULT, 1, 0, 2)) {
		WiFi.softAPdisconnect(true);
		WiFi.mode(WIFI_OFF);
		wifiEnabled = false;
		updateWifiLed();
		return WifiStatus::ApStartFailed;
	}

	server.begin();
	wifiEnabled = true;
	updateWifiLed();
	return WifiStatus::Success;
}

inline WifiStatus disableWiFi() {
	if (!wifiEnabled) {
		updateWifiLed();
		return WifiStatus::Disabled;
	}

	server.stop();
	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_OFF);
	wifiEnabled = false;
	updateWifiLed();
	return WifiStatus::Disabled;
}

inline WifiStatus toggleWiFi() {
	if (wifiEnabled) {
		return disableWiFi();
	}
	return enableWiFi();
}

inline bool isWiFiEnabled() { return wifiEnabled; }

inline void initWiFi() {
	pinMode(WIFI_LED_PIN, OUTPUT);
	updateWifiLed();
}
