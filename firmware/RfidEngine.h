#pragma once
#include "AudioEngine.h"
#include "Config.h"
#include "Storage.h"
#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <map>

struct DiscMapping {
	String uid;
	String type;
	String target;
	String label;
};

static MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
static std::map<String, DiscMapping> g_discMappings;

static String g_rfidCurrentUid = "";
static bool g_rfidIsPresent = false;
static int g_rfidMissedPings = 0;
static bool g_rfidAudioPlaying = false;

inline void loadDiscMappings() {
	SdGuard guard;
	g_discMappings.clear();
	if (!SD.exists("/discs.csv")) {
		return;
	}
	File f = SD.open("/discs.csv", FILE_READ);
	if (!f) {
		return;
	}
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() == 0) {
			continue;
		}
		int p1 = line.indexOf('|');
		if (p1 == -1) {
			continue;
		}
		int p2 = line.indexOf('|', p1 + 1);
		if (p2 == -1) {
			continue;
		}
		int p3 = line.indexOf('|', p2 + 1);
		if (p3 == -1) {
			continue;
		}
		String uid = line.substring(0, p1);
		String type = line.substring(p1 + 1, p2);
		String target = line.substring(p2 + 1, p3);
		String label = line.substring(p3 + 1);
		g_discMappings[uid] = {uid, type, target, label};
	}
	f.close();
}

inline void saveDiscMappings() {
	SdGuard guard;
	File f = SD.open("/discs.csv", FILE_WRITE);
	if (!f) {
		return;
	}
	for (const auto &pair : g_discMappings) {
		f.print(pair.second.uid);
		f.print("|");
		f.print(pair.second.type);
		f.print("|");
		f.print(pair.second.target);
		f.print("|");
		f.println(pair.second.label);
	}
	f.close();
}

inline bool mapDisc(const String &uid, const String &type, const String &target,
					const String &label) {
	if (uid.length() == 0 || (type != "track" && type != "playlist") || target.length() == 0) {
		return false;
	}
	g_discMappings[uid] = {uid, type, target, label.length() > 0 ? label : target};
	saveDiscMappings();
	return true;
}

inline bool unmapDisc(const String &uid) {
	if (g_discMappings.erase(uid) > 0) {
		saveDiscMappings();
		return true;
	}
	return false;
}

inline bool isRfidPresent() { return g_rfidIsPresent; }

inline String getRfidUid() { return g_rfidCurrentUid; }

inline bool isRfidMapped() { return g_discMappings.find(g_rfidCurrentUid) != g_discMappings.end(); }

inline String getRfidType() {
	auto it = g_discMappings.find(g_rfidCurrentUid);
	if (it != g_discMappings.end()) {
		return it->second.type;
	}
	return "";
}

inline String getRfidTarget() {
	auto it = g_discMappings.find(g_rfidCurrentUid);
	if (it != g_discMappings.end()) {
		return it->second.target;
	}
	return "";
}

inline String getRfidLabel() {
	auto it = g_discMappings.find(g_rfidCurrentUid);
	if (it != g_discMappings.end()) {
		return it->second.label;
	}
	return "";
}

inline bool scanCardPresence(String &outUid) {
	byte bufferATQA[2];
	byte bufferSize = sizeof(bufferATQA);
	MFRC522::StatusCode status = rfid.PICC_RequestA(bufferATQA, &bufferSize);
	if (status != MFRC522::STATUS_OK) {
		status = rfid.PICC_WakeupA(bufferATQA, &bufferSize);
	}
	if (status != MFRC522::STATUS_OK) {
		return false;
	}
	if (!rfid.PICC_ReadCardSerial()) {
		return false;
	}
	String uid = "";
	for (byte i = 0; i < rfid.uid.size; i++) {
		if (rfid.uid.uidByte[i] < 0x10) {
			uid += "0";
		}
		uid += String(rfid.uid.uidByte[i], HEX);
	}
	uid.toUpperCase();
	outUid = uid;
	rfid.PICC_HaltA();
	rfid.PCD_StopCrypto1();
	return true;
}

inline void handleRfidPoll() {
	String detectedUid = "";
	bool found = scanCardPresence(detectedUid);
	if (found) {
		if (!g_rfidIsPresent || detectedUid != g_rfidCurrentUid) {
			Serial.print("[RFID] Disc detected UID: ");
			Serial.println(detectedUid);

			stopAudio();
			g_rfidAudioPlaying = false;
			g_rfidCurrentUid = detectedUid;
			g_rfidIsPresent = true;
			g_rfidMissedPings = 0;

			auto it = g_discMappings.find(detectedUid);
			if (it != g_discMappings.end()) {
				Serial.print("[RFID] Mapped to ");
				Serial.print(it->second.type);
				Serial.print(": ");
				Serial.println(it->second.target);

				if (it->second.type == "track") {
					if (startTrack(it->second.target)) {
						g_rfidAudioPlaying = true;
					}
				} else if (it->second.type == "playlist") {
					String firstTrack = getPlaylistFirstTrack(it->second.target);
					if (firstTrack.length() > 0 && startTrack(firstTrack)) {
						g_rfidAudioPlaying = true;
					}
				}
			} else {
				Serial.println("[RFID] Unmapped disc");
			}
		} else {
			g_rfidMissedPings = 0;
		}
	} else {
		if (g_rfidIsPresent) {
			g_rfidMissedPings++;
			if (g_rfidMissedPings >= 3) {
				Serial.print("[RFID] Disc removed: ");
				Serial.println(g_rfidCurrentUid);

				if (g_rfidAudioPlaying) {
					stopAudio();
					g_rfidAudioPlaying = false;
				}
				g_rfidIsPresent = false;
				g_rfidCurrentUid = "";
				g_rfidMissedPings = 0;
			}
		}
	}
}

inline void rfidTaskLoop(void *param) {
	for (;;) {
		handleRfidPoll();
		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

inline void initRfid() {
	pinMode(RFID_SS_PIN, OUTPUT);
	digitalWrite(RFID_SS_PIN, HIGH);
	SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
	rfid.PCD_Init();
	loadDiscMappings();
	Serial.println("[RFID] Reader initialized.");
}

inline void startRfidTask() {
	xTaskCreatePinnedToCore(rfidTaskLoop, "rfidTask", 4096, nullptr, 2, nullptr, 1);
}
