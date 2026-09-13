#pragma once
#include "AudioEngine.h"
#include "Config.h"
#include "Storage.h"
#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>

struct DiscMappingRecord {
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

static DiscMappingRecord g_rfidActiveMapping;
static bool g_rfidIsPresent = false;
static int g_rfidMissedPings = 0;
static bool g_rfidAudioPlaying = false;
static bool g_rfidEngineEnabled = true;
static unsigned long g_lastRfidPollTime = 0;

inline void setRfidEnabled(bool enabled) {
	g_rfidEngineEnabled = enabled;
	if (!enabled) {
		g_rfidIsPresent = false;
		g_rfidActiveMapping = DiscMappingRecord();
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

inline bool parseMappingLine(const String &line, DiscMappingRecord &record) {
	int p1 = line.indexOf('|');
	int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
	int p3 = (p2 != -1) ? line.indexOf('|', p2 + 1) : -1;
	if (p1 != -1 && p2 != -1 && p3 != -1) {
		record.uid = line.substring(0, p1);
		record.type = line.substring(p1 + 1, p2);
		record.target = line.substring(p2 + 1, p3);
		record.label = line.substring(p3 + 1);
		return (record.uid.length() > 0 && record.target.length() > 0);
	}
	return false;
}

inline bool findDiscMappingOnSd(const String &searchUid, DiscMappingRecord &outRecord) {
	if (!isStorageReady()) {
		return false;
	}
	SdGuard guard;
	if (!SD.exists("/discs.csv")) {
		return false;
	}
	File f = SD.open("/discs.csv", FILE_READ);
	if (!f) {
		return false;
	}
	bool found = false;
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() == 0)
			continue;
		DiscMappingRecord rec;
		if (parseMappingLine(line, rec) && rec.uid == searchUid) {
			outRecord = rec;
			found = true;
			break;
		}
	}
	f.close();
	return found;
}

inline StorageResult mapDisc(const String &rawUid, const String &rawType, const String &rawTarget,
							 const String &rawLabel) {
	heapLog("mapDisc:enter");
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

	String destPath = "/discs.csv";
	String tempPath = "/discs.tmp";

	{
		SdGuard guard;
		File inF = SD.open(destPath, FILE_READ);
		File outF = SD.open(tempPath, FILE_WRITE);
		if (!outF) {
			if (inF)
				inF.close();
			return StorageResult::Fail;
		}

		bool overwritten = false;
		if (inF) {
			while (inF.available()) {
				String line = inF.readStringUntil('\n');
				line.trim();
				if (line.length() == 0)
					continue;
				DiscMappingRecord rec;
				if (parseMappingLine(line, rec)) {
					if (rec.uid == uid) {
						outF.print(uid);
						outF.print('|');
						outF.print(type);
						outF.print('|');
						outF.print(target);
						outF.print('|');
						outF.println(label.length() > 0 ? label : target);
						overwritten = true;
					} else {
						outF.println(line);
					}
				}
			}
			inF.close();
		}

		if (!overwritten) {
			outF.print(uid);
			outF.print('|');
			outF.print(type);
			outF.print('|');
			outF.print(target);
			outF.print('|');
			outF.println(label.length() > 0 ? label : target);
		}
		outF.close();

		StorageResult res = replaceFileSafelyUnderLock(tempPath, destPath);
		if (res != StorageResult::Ok) {
			return res;
		}
	}

	if (g_rfidEngineEnabled && g_rfidIsPresent && g_rfidActiveMapping.uid == uid) {
		g_rfidActiveMapping.type = type;
		g_rfidActiveMapping.target = target;
		g_rfidActiveMapping.label = label.length() > 0 ? label : target;

		requestAudioStop();
		g_rfidAudioPlaying = false;
		if (type == "track") {
			if (requestAudioPlay(target)) {
				g_rfidAudioPlaying = true;
			}
		} else if (type == "playlist") {
			if (requestAudioPlayPlaylist(target)) {
				g_rfidAudioPlaying = true;
			}
		}
	}

	heapLog("mapDisc:done");
	return StorageResult::Ok;
}

inline StorageResult unmapDisc(const String &rawUid) {
	heapLog("unmapDisc:enter");
	if (!isStorageReady()) {
		return StorageResult::StorageUnavailable;
	}

	String uid = sanitizeField(rawUid);
	String destPath = "/discs.csv";
	String tempPath = "/discs.tmp";

	bool found = false;
	{
		SdGuard guard;
		File inF = SD.open(destPath, FILE_READ);
		if (!inF) {
			return StorageResult::NotFound;
		}
		File outF = SD.open(tempPath, FILE_WRITE);
		if (!outF) {
			inF.close();
			return StorageResult::Fail;
		}

		while (inF.available()) {
			String line = inF.readStringUntil('\n');
			line.trim();
			if (line.length() == 0)
				continue;
			DiscMappingRecord rec;
			if (parseMappingLine(line, rec)) {
				if (rec.uid == uid) {
					found = true;
				} else {
					outF.println(line);
				}
			}
		}
		inF.close();
		outF.close();

		if (!found) {
			SD.remove(tempPath);
			return StorageResult::NotFound;
		}

		StorageResult res = replaceFileSafelyUnderLock(tempPath, destPath);
		if (res != StorageResult::Ok) {
			return res;
		}
	}

	if (g_rfidActiveMapping.uid == uid) {
		g_rfidActiveMapping.type = "";
		g_rfidActiveMapping.target = "";
		g_rfidActiveMapping.label = "";
	}

	heapLog("unmapDisc:done");
	return StorageResult::Ok;
}

inline bool cascadeRenamePlaylist(const String &oldName, const String &newName) {
	if (!isStorageReady()) {
		return false;
	}
	String destPath = "/discs.csv";
	String tempPath = "/discs.tmp";

	bool changed = false;
	{
		SdGuard guard;
		File inF = SD.open(destPath, FILE_READ);
		if (!inF) {
			return true;
		}
		File outF = SD.open(tempPath, FILE_WRITE);
		if (!outF) {
			inF.close();
			return false;
		}

		while (inF.available()) {
			String line = inF.readStringUntil('\n');
			line.trim();
			if (line.length() == 0)
				continue;
			DiscMappingRecord rec;
			if (parseMappingLine(line, rec)) {
				if (rec.type == "playlist" && rec.target == oldName) {
					rec.target = newName;
					if (rec.label == oldName) {
						rec.label = newName;
					}
					outF.print(rec.uid);
					outF.print('|');
					outF.print(rec.type);
					outF.print('|');
					outF.print(rec.target);
					outF.print('|');
					outF.println(rec.label);
					changed = true;
				} else {
					outF.println(line);
				}
			}
		}
		inF.close();
		outF.close();

		if (!changed) {
			SD.remove(tempPath);
			return true;
		}

		return (replaceFileSafelyUnderLock(tempPath, destPath) == StorageResult::Ok);
	}
}

inline bool cascadeDeletePlaylist(const String &name) {
	if (!isStorageReady()) {
		return false;
	}
	String destPath = "/discs.csv";
	String tempPath = "/discs.tmp";

	bool changed = false;
	{
		SdGuard guard;
		File inF = SD.open(destPath, FILE_READ);
		if (!inF) {
			return true;
		}
		File outF = SD.open(tempPath, FILE_WRITE);
		if (!outF) {
			inF.close();
			return false;
		}

		while (inF.available()) {
			String line = inF.readStringUntil('\n');
			line.trim();
			if (line.length() == 0)
				continue;
			DiscMappingRecord rec;
			if (parseMappingLine(line, rec)) {
				if (rec.type == "playlist" && rec.target == name) {
					changed = true;
				} else {
					outF.println(line);
				}
			}
		}
		inF.close();
		outF.close();

		if (!changed) {
			SD.remove(tempPath);
			return true;
		}

		return (replaceFileSafelyUnderLock(tempPath, destPath) == StorageResult::Ok);
	}
}

inline RfidStateSnapshot getRfidSnapshot() {
	RfidStateSnapshot snap;
	snap.present = g_rfidIsPresent;
	snap.mapped = false;
	strncpy(snap.uid, g_rfidActiveMapping.uid.c_str(), sizeof(snap.uid) - 1);
	snap.uid[sizeof(snap.uid) - 1] = '\0';
	snap.type[0] = '\0';
	snap.target[0] = '\0';
	snap.label[0] = '\0';

	if (g_rfidIsPresent && g_rfidActiveMapping.uid.length() > 0 &&
		g_rfidActiveMapping.target.length() > 0) {
		snap.mapped = true;
		strncpy(snap.type, g_rfidActiveMapping.type.c_str(), sizeof(snap.type) - 1);
		snap.type[sizeof(snap.type) - 1] = '\0';
		strncpy(snap.target, g_rfidActiveMapping.target.c_str(), sizeof(snap.target) - 1);
		snap.target[sizeof(snap.target) - 1] = '\0';
		strncpy(snap.label, g_rfidActiveMapping.label.c_str(), sizeof(snap.label) - 1);
		snap.label[sizeof(snap.label) - 1] = '\0';
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
	if (!g_rfidEngineEnabled) {
		return;
	}

	String detectedUid = "";
	bool found = scanCardPresence(detectedUid);
	if (found) {
		bool isNew = false;
		if (!g_rfidIsPresent || detectedUid != g_rfidActiveMapping.uid) {
			isNew = true;
			g_rfidActiveMapping = DiscMappingRecord();
			g_rfidActiveMapping.uid = detectedUid;
			g_rfidIsPresent = true;
			g_rfidMissedPings = 0;
			g_rfidAudioPlaying = false;
			findDiscMappingOnSd(detectedUid, g_rfidActiveMapping);
		} else {
			g_rfidMissedPings = 0;
		}

		if (isNew) {
			heapLog("RFID:disc DETECTED (new)");
			requestAudioStop();
			heapLog("RFID:after requestAudioStop");
			if (g_rfidActiveMapping.target.length() > 0) {
				if (g_rfidActiveMapping.type == "track") {
					heapLog("RFID:before requestAudioPlay(track)");
					if (requestAudioPlay(g_rfidActiveMapping.target)) {
						g_rfidAudioPlaying = true;
					}
					heapLog("RFID:after requestAudioPlay(track)");
				} else if (g_rfidActiveMapping.type == "playlist") {
					heapLog("RFID:before requestAudioPlayPlaylist");
					if (requestAudioPlayPlaylist(g_rfidActiveMapping.target)) {
						g_rfidAudioPlaying = true;
					}
					heapLog("RFID:after requestAudioPlayPlaylist");
				}
			} else {
				heapLog("RFID:disc detected but NOT mapped");
			}
		}
	} else {
		bool shouldStop = false;
		if (g_rfidIsPresent) {
			g_rfidMissedPings++;
			if (g_rfidMissedPings >= 3) {
				g_rfidIsPresent = false;
				g_rfidActiveMapping = DiscMappingRecord();
				g_rfidMissedPings = 0;
				if (g_rfidAudioPlaying) {
					g_rfidAudioPlaying = false;
					shouldStop = true;
				}
			}
		}
		if (shouldStop) {
			heapLog("RFID:disc REMOVED, stopping audio");
			requestAudioStop();
			heapLog("RFID:after stop-on-removal");
		}
	}
}

inline void handleRfidLoop() {
	unsigned long now = millis();
	if (now - g_lastRfidPollTime >= 120) {
		g_lastRfidPollTime = now;
		handleRfidPoll();
	}
}

inline bool initRfid() {
	pinMode(RFID_SS_PIN, OUTPUT);
	digitalWrite(RFID_SS_PIN, HIGH);
	SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
	rfid.PCD_Init();
	return true;
}
