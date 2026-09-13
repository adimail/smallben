#pragma once

#include "Config.h"
#include "SmallbenServer.h"
#include <Arduino.h>
#include <WiFi.h>

enum class WifiStatus { Success, BlockedByBluetooth, ApStartFailed, Disabled };

static bool wifiEnabled = false;

inline void updateWifiLed() { digitalWrite(WIFI_LED_PIN, wifiEnabled ? HIGH : LOW); }

inline WifiStatus enableWiFi() {
	heapLog("enableWiFi:enter");
	if (wifiEnabled) {
		updateWifiLed();
		return WifiStatus::Success;
	}
	extern bool isBluetoothActive();
	if (isBluetoothActive()) {
		wifiEnabled = false;
		updateWifiLed();
		heapLog("enableWiFi:blockedByBT");
		return WifiStatus::BlockedByBluetooth;
	}
	vTaskDelay(pdMS_TO_TICKS(20));
	WiFi.mode(WIFI_AP);
	heapLog("enableWiFi:after WIFI_AP mode set");

	if (!WiFi.softAP(AP_SSID, AP_PASS_DEFAULT, 1, 0, 2)) {
		WiFi.softAPdisconnect(true);
		WiFi.mode(WIFI_OFF);
		wifiEnabled = false;
		updateWifiLed();
		heapLog("enableWiFi:softAP FAILED");
		return WifiStatus::ApStartFailed;
	}
	heapLog("enableWiFi:after softAP success");

	server.begin();
	wifiEnabled = true;
	updateWifiLed();
	heapLog("enableWiFi:after server.begin");
	return WifiStatus::Success;
}

inline WifiStatus disableWiFi() {
	heapLog("disableWiFi:enter");
	if (!wifiEnabled) {
		updateWifiLed();
		return WifiStatus::Disabled;
	}
	server.stop();
	heapLog("disableWiFi:after server.stop");
	WiFi.softAPdisconnect(true);
	heapLog("disableWiFi:after softAPdisconnect");
	WiFi.mode(WIFI_OFF);
	heapLog("disableWiFi:after WIFI_OFF");
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
