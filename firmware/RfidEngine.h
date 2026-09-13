#pragma once
#include "AudioEngine.h"
#include "Config.h"
#include "Storage.h"
#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <map>
#include <vector>

struct DiscMapping {
	String uid;
	String type;
	String target;
	String label;
};

struct RfidStateSnapshot {
	bool present;
	bool mapped;
	char uid[32];
	char type[16];
	char target[128];
	char label[64];
};

static MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
static SemaphoreHandle_t rfidMutex = nullptr;
static TaskHandle_t rfidTaskHandle = nullptr;

static std::map<String, DiscMapping> g_discMappings;
static String g_rfidCurrentUid = "";
static bool g_rfidIsPresent = false;
static int g_rfidMissedPings = 0;
static bool g_rfidAudioPlaying = false;
static bool g_rfidEngineEnabled = true;

inline void ensureRfidMutex() {
	if (!rfidMutex) {
		rfidMutex = xSemaphoreCreateMutex();
	}
}

class RfidGuard {
  public:
	RfidGuard() {
		ensureRfidMutex();
		if (rfidMutex) {
			xSemaphoreTake(rfidMutex, portMAX_DELAY);
		}
	}
	~RfidGuard() {
		if (rfidMutex) {
			xSemaphoreGive(rfidMutex);
		}
	}
	RfidGuard(const RfidGuard &) = delete;
	RfidGuard &operator=(const RfidGuard &) = delete;
};

inline void setRfidEnabled(bool enabled) {
	RfidGuard rGuard;
	g_rfidEngineEnabled = enabled;
	if (!enabled) {
		g_rfidIsPresent = false;
		g_rfidCurrentUid = "";
		g_rfidMissedPings = 0;
		g_rfidAudioPlaying = false;
	}
}

inline String sanitizeField(const String &input) {
	String out = input;
	out.replace("|", "-");
	out.replace("\r", "");
	out.replace("\n", "");
	out.trim();
	return out;
}

inline void loadDiscMappings() {
	if (!isStorageReady()) {
		RfidGuard rGuard;
		g_discMappings.clear();
		return;
	}
	String content = "";
	StorageResult res = readStorageText("/discs.csv", content);
	if (res != StorageResult::Ok) {
		RfidGuard rGuard;
		g_discMappings.clear();
		return;
	}

	std::map<String, DiscMapping> loaded;
	int start = 0;
	while (start < (int)content.length()) {
		int end = content.indexOf('\n', start);
		if (end == -1)
			end = content.length();
		String line = content.substring(start, end);
		line.trim();
		start = end + 1;
		if (line.length() == 0)
			continue;
		int p1 = line.indexOf('|');
		int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
		int p3 = (p2 != -1) ? line.indexOf('|', p2 + 1) : -1;
		if (p1 != -1 && p2 != -1 && p3 != -1) {
			String uid = line.substring(0, p1);
			String type = line.substring(p1 + 1, p2);
			String target = line.substring(p2 + 1, p3);
			String label = line.substring(p3 + 1);
			if (uid.length() > 0 && target.length() > 0) {
				loaded[uid] = {uid, type, target, label};
			}
		}
	}
	RfidGuard rGuard;
	g_discMappings = std::move(loaded);
}

inline String serializeMappings(const std::map<String, DiscMapping> &mappings) {
	String out = "";
	for (const auto &pair : mappings) {
		out += pair.second.uid + "|" + pair.second.type + "|" + pair.second.target + "|" +
			   pair.second.label + "\n";
	}
	return out;
}

inline StorageResult commitMappingsAtomically(const std::map<String, DiscMapping> &newMap) {
	String serialized = serializeMappings(newMap);
	StorageResult res = writeTextAtomic("/discs.csv", serialized);
	if (res == StorageResult::Ok) {
		RfidGuard rGuard;
		g_discMappings = newMap;
	}
	return res;
}

inline StorageResult mapDisc(const String &rawUid, const String &rawType, const String &rawTarget,
							 const String &rawLabel) {
	if (!isStorageReady()) {
		return StorageResult::StorageUnavailable;
	}

	String uid = sanitizeField(rawUid);
	String type = sanitizeField(rawType);
	String target = sanitizeField(rawTarget);
	String label = sanitizeField(rawLabel);

	if (uid.length() == 0 || (type != "track" && type != "playlist") || target.length() == 0) {
		return StorageResult::InvalidPath;
	}

	if (type == "track") {
		if (!isValidPath(target) || !isMp3(target) || !storageExists(target)) {
			return StorageResult::NotFound;
		}
	} else if (type == "playlist") {
		if (!isValidPlaylistName(target) || !storageExists(getPlaylistPath(target))) {
			return StorageResult::NotFound;
		}
	}

	std::map<String, DiscMapping> staging;
	{
		RfidGuard rGuard;
		staging = g_discMappings;
	}
	staging[uid] = {uid, type, target, label.length() > 0 ? label : target};

	StorageResult res = commitMappingsAtomically(staging);
	if (res != StorageResult::Ok) {
		return res;
	}

	bool shouldPlayNow = false;
	{
		RfidGuard rGuard;
		if (g_rfidEngineEnabled && g_rfidIsPresent && g_rfidCurrentUid == uid &&
			!g_rfidAudioPlaying) {
			shouldPlayNow = true;
		}
	}

	if (shouldPlayNow) {
		requestAudioStop();
		if (type == "track") {
			if (requestAudioPlay(target)) {
				RfidGuard rGuard;
				g_rfidAudioPlaying = true;
			}
		} else if (type == "playlist") {
			if (requestAudioPlayPlaylist(target)) {
				RfidGuard rGuard;
				g_rfidAudioPlaying = true;
			}
		}
	}

	return StorageResult::Ok;
}

inline StorageResult unmapDisc(const String &rawUid) {
	if (!isStorageReady()) {
		return StorageResult::StorageUnavailable;
	}

	String uid = sanitizeField(rawUid);
	std::map<String, DiscMapping> staging;
	{
		RfidGuard rGuard;
		auto it = g_discMappings.find(uid);
		if (it == g_discMappings.end()) {
			return StorageResult::NotFound;
		}
		staging = g_discMappings;
		staging.erase(uid);
	}

	return commitMappingsAtomically(staging);
}

inline bool cascadeRenamePlaylist(const String &oldName, const String &newName) {
	if (!isStorageReady()) {
		return false;
	}
	std::map<String, DiscMapping> staging;
	bool changed = false;
	{
		RfidGuard rGuard;
		staging = g_discMappings;
		for (auto &pair : staging) {
			if (pair.second.type == "playlist" && pair.second.target == oldName) {
				pair.second.target = newName;
				if (pair.second.label == oldName) {
					pair.second.label = newName;
				}
				changed = true;
			}
		}
	}
	if (!changed)
		return true;
	return commitMappingsAtomically(staging) == StorageResult::Ok;
}

inline bool cascadeDeletePlaylist(const String &name) {
	if (!isStorageReady()) {
		return false;
	}
	std::map<String, DiscMapping> staging;
	bool changed = false;
	{
		RfidGuard rGuard;
		staging = g_discMappings;
		for (auto it = staging.begin(); it != staging.end();) {
			if (it->second.type == "playlist" && it->second.target == name) {
				it = staging.erase(it);
				changed = true;
			} else {
				++it;
			}
		}
	}
	if (!changed)
		return true;
	return commitMappingsAtomically(staging) == StorageResult::Ok;
}

inline std::vector<DiscMapping> getDiscMappingsSnapshot() {
	RfidGuard rGuard;
	std::vector<DiscMapping> list;
	list.reserve(g_discMappings.size());
	for (const auto &pair : g_discMappings) {
		list.push_back(pair.second);
	}
	return list;
}

inline RfidStateSnapshot getRfidSnapshot() {
	RfidGuard rGuard;
	RfidStateSnapshot snap;
	snap.present = g_rfidIsPresent;
	snap.mapped = false;
	strncpy(snap.uid, g_rfidCurrentUid.c_str(), sizeof(snap.uid) - 1);
	snap.uid[sizeof(snap.uid) - 1] = '\0';
	snap.type[0] = '\0';
	snap.target[0] = '\0';
	snap.label[0] = '\0';

	if (g_rfidIsPresent && g_rfidCurrentUid.length() > 0) {
		auto it = g_discMappings.find(g_rfidCurrentUid);
		if (it != g_discMappings.end()) {
			snap.mapped = true;
			strncpy(snap.type, it->second.type.c_str(), sizeof(snap.type) - 1);
			snap.type[sizeof(snap.type) - 1] = '\0';
			strncpy(snap.target, it->second.target.c_str(), sizeof(snap.target) - 1);
			snap.target[sizeof(snap.target) - 1] = '\0';
			strncpy(snap.label, it->second.label.c_str(), sizeof(snap.label) - 1);
			snap.label[sizeof(snap.label) - 1] = '\0';
		}
	}
	return snap;
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
	{
		RfidGuard rGuard;
		if (!g_rfidEngineEnabled) {
			return;
		}
	}

	String detectedUid = "";
	bool found = scanCardPresence(detectedUid);
	if (found) {
		bool isNew = false;
		DiscMapping mapping;
		bool hasMapping = false;
		{
			RfidGuard rGuard;
			if (!g_rfidEngineEnabled) {
				return;
			}
			if (!g_rfidIsPresent || detectedUid != g_rfidCurrentUid) {
				isNew = true;
				g_rfidCurrentUid = detectedUid;
				g_rfidIsPresent = true;
				g_rfidMissedPings = 0;
				g_rfidAudioPlaying = false;
				auto it = g_discMappings.find(detectedUid);
				if (it != g_discMappings.end()) {
					mapping = it->second;
					hasMapping = true;
				}
			} else {
				g_rfidMissedPings = 0;
			}
		}

		if (isNew) {
			requestAudioStop();
			if (hasMapping) {
				if (mapping.type == "track") {
					if (requestAudioPlay(mapping.target)) {
						RfidGuard rGuard;
						g_rfidAudioPlaying = true;
					}
				} else if (mapping.type == "playlist") {
					if (requestAudioPlayPlaylist(mapping.target)) {
						RfidGuard rGuard;
						g_rfidAudioPlaying = true;
					}
				}
			}
		}
	} else {
		bool shouldStop = false;
		{
			RfidGuard rGuard;
			if (g_rfidIsPresent) {
				g_rfidMissedPings++;
				if (g_rfidMissedPings >= 3) {
					g_rfidIsPresent = false;
					g_rfidCurrentUid = "";
					g_rfidMissedPings = 0;
					if (g_rfidAudioPlaying) {
						g_rfidAudioPlaying = false;
						shouldStop = true;
					}
				}
			}
		}
		if (shouldStop) {
			requestAudioStop();
		}
	}
}

inline void rfidTaskLoop(void *param) {
	for (;;) {
		handleRfidPoll();
		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

inline bool initRfid() {
	ensureRfidMutex();
	if (!rfidMutex) {
		return false;
	}
	pinMode(RFID_SS_PIN, OUTPUT);
	digitalWrite(RFID_SS_PIN, HIGH);
	SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
	rfid.PCD_Init();
	loadDiscMappings();
	return true;
}

inline bool startRfidTask() {
	if (!rfidTaskHandle) {
		BaseType_t res =
			xTaskCreatePinnedToCore(rfidTaskLoop, "rfidTask", 3072, nullptr, 2, &rfidTaskHandle, 1);
		return (res == pdPASS);
	}
	return true;
}
