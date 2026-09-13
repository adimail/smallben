#pragma once
#include "AudioEngine.h"
#include "BluetoothEngine.h"
#include "Config.h"
#include "Dashboard.h"
#include "RfidEngine.h"
#include "Storage.h"
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

WebServer server(HTTP_PORT);

static File uploadFile;
static bool uploadAborted = false;
static String uploadError = "";
static size_t uploadBytesWritten = 0;
static String uploadFinalPath = "";
static String uploadTempPath = "";

inline void sendStorageResult(StorageResult res, const String &successMsg = "{\"status\":\"ok\"}") {
	switch (res) {
	case StorageResult::Ok:
		server.send(200, "application/json", successMsg);
		break;
	case StorageResult::NotFound:
		server.send(404, "application/json", "{\"error\":\"not found\"}");
		break;
	case StorageResult::AlreadyExists:
		server.send(409, "application/json", "{\"error\":\"already exists\"}");
		break;
	case StorageResult::InvalidPath:
		server.send(400, "application/json", "{\"error\":\"invalid path or name\"}");
		break;
	case StorageResult::Protected:
		server.send(403, "application/json", "{\"error\":\"path is protected\"}");
		break;
	case StorageResult::Active:
		server.send(409, "application/json",
					"{\"error\":\"item is currently active in playback\"}");
		break;
	case StorageResult::NoSpace:
		server.send(507, "application/json", "{\"error\":\"storage full\"}");
		break;
	case StorageResult::DirectoryNotEmpty:
		server.send(409, "application/json", "{\"error\":\"directory not empty\"}");
		break;
	case StorageResult::StorageUnavailable:
		server.send(503, "application/json", "{\"error\":\"SD card unavailable\"}");
		break;
	default:
		server.send(500, "application/json", "{\"error\":\"storage operation failed\"}");
		break;
	}
}

inline String jsonEscape(const String &in) {
	String out;
	out.reserve(in.length() + 8);
	for (size_t i = 0; i < in.length(); i++) {
		char c = in[i];
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\b':
			out += "\\b";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if ((uint8_t)c < 0x20) {
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
				out += buf;
			} else {
				out += c;
			}
			break;
		}
	}
	return out;
}

inline String extractJsonString(const String &json, const String &key) {
	String needle = "\"" + key + "\"";
	int kpos = json.indexOf(needle);
	if (kpos == -1)
		return "";
	int colon = json.indexOf(':', kpos + needle.length());
	if (colon == -1)
		return "";
	int q1 = json.indexOf('"', colon + 1);
	if (q1 == -1)
		return "";
	String result = "";
	bool escape = false;
	for (int i = q1 + 1; i < (int)json.length(); i++) {
		char c = json[i];
		if (escape) {
			if (c == '"' || c == '\\' || c == '/')
				result += c;
			else if (c == 'b')
				result += '\b';
			else if (c == 'f')
				result += '\f';
			else if (c == 'n')
				result += '\n';
			else if (c == 'r')
				result += '\r';
			else if (c == 't')
				result += '\t';
			else
				result += c;
			escape = false;
		} else if (c == '\\') {
			escape = true;
		} else if (c == '"') {
			return result;
		} else {
			result += c;
		}
	}
	return "";
}

inline void handleRoot() { streamDashboard(server); }

inline void handleNow() {
	AudioStateSnapshot audio = getAudioSnapshot();
	RfidStateSnapshot rfid = getRfidSnapshot();

	String pathStr(audio.path);
	String nameStr = "";
	int slash = pathStr.lastIndexOf('/');
	if (slash != -1) {
		nameStr = pathStr.substring(slash + 1);
	} else {
		nameStr = pathStr;
	}

	String stateStr = "stopped";
	if (isBluetoothActive()) {
		stateStr = isBluetoothConnected() ? "bluetooth_connected" : "bluetooth_pairing";
	} else if (audio.valid && audio.isPlaying) {
		stateStr = audio.isPaused ? "paused" : "playing";
	}

	String json = "{";
	json += "\"mode\":\"" + String(isBluetoothActive() ? "bluetooth" : "disc") + "\",";
	json += "\"player\":{";
	json += "\"state\":\"" + stateStr + "\",";
	json += "\"path\":\"" + jsonEscape(pathStr) + "\",";
	json += "\"name\":\"" + jsonEscape(nameStr) + "\"";
	json += "},";
	json += "\"sd\":{";
	json += "\"ready\":" + String(isStorageReady() ? "true" : "false") + ",";
	json += "\"totalBytes\":" + String((unsigned long long)getSdTotalBytes()) + ",";
	json += "\"usedBytes\":" + String((unsigned long long)getSdUsedBytes());
	json += "},";
	json += "\"rfid\":{";
	json += "\"present\":" + String(rfid.present ? "true" : "false") + ",";
	json += "\"uid\":\"" + jsonEscape(String(rfid.uid)) + "\",";
	json += "\"mapped\":" + String(rfid.mapped ? "true" : "false") + ",";
	json += "\"type\":\"" + jsonEscape(String(rfid.type)) + "\",";
	json += "\"target\":\"" + jsonEscape(String(rfid.target)) + "\",";
	json += "\"label\":\"" + jsonEscape(String(rfid.label)) + "\"";
	json += "},";
	json += "\"system\":{";
	json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
	json += "\"uptime\":" + String(millis() / 1000);
	json += "}";
	json += "}";

	server.send(200, "application/json", json);
}

inline void handleApiDiscsGet() {
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	streamDiscMappingsFromSd([&](const DiscMappingRecord &item) {
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"uid\":\"" + jsonEscape(item.uid) + "\",\"type\":\"" +
						   jsonEscape(item.type) + "\",\"target\":\"" + jsonEscape(item.target) +
						   "\",\"label\":\"" + jsonEscape(item.label) + "\"}");
	});
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiDiscsMap() {
	heapLog("handleApiDiscsMap:enter");
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String body = server.arg("plain");
	String uid = extractJsonString(body, "uid");
	String type = extractJsonString(body, "type");
	String target = extractJsonString(body, "target");
	String label = extractJsonString(body, "label");

	StorageResult res = mapDisc(uid, type, target, label);
	heapLog("handleApiDiscsMap:after mapDisc");
	sendStorageResult(res);
}

inline void handleApiDiscsDelete() {
	heapLog("handleApiDiscsDelete:enter");
	String uid = "";
	if (server.hasArg("uid")) {
		uid = server.arg("uid");
	} else if (server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			server.send(413, "application/json", "{\"error\":\"payload too large\"}");
			return;
		}
		uid = extractJsonString(server.arg("plain"), "uid");
	}
	StorageResult res = unmapDisc(uid);
	heapLog("handleApiDiscsDelete:after unmapDisc");
	sendStorageResult(res);
}

inline void streamMp3Entries(const String &dirPath, bool &first) {
	SdGuard guard;
	File dir = SD.open(dirPath);
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		return;
	}

	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;
		String name = entry.name();
		bool isDir = entry.isDirectory();
		size_t sz = entry.size();
		entry.close();

		if (isSystemOrHidden(name))
			continue;

		String fullPath = (dirPath == "/" ? "" : dirPath) + "/" + name;
		if (isDir) {
			streamMp3Entries(fullPath, first);
		} else if (isMp3(name)) {
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("{\"name\":\"" + jsonEscape(name) + "\",\"path\":\"" +
							   jsonEscape(fullPath) + "\",\"size\":" + String(sz) + "}");
		}
	}
	dir.close();
}

inline void handleApiMp3() {
	heapLog("handleApiMp3:enter");
	if (!isStorageReady()) {
		server.send(503, "application/json", "[]");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	streamMp3Entries("/", first);
	server.sendContent("]");
	server.sendContent("");
	heapLog("handleApiMp3:done");
}

inline void handleApiTrackPlaylists() {
	if (!server.hasArg("path")) {
		server.send(400, "application/json", "{\"error\":\"missing path\"}");
		return;
	}
	String path = server.arg("path");
	if (!isValidPath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;

	if (isStorageReady()) {
		SdGuard guard;
		File dir = SD.open("/playlists");
		if (dir && dir.isDirectory()) {
			while (true) {
				File entry = dir.openNextFile();
				if (!entry)
					break;
				String name = entry.name();
				bool isDir = entry.isDirectory();
				entry.close();

				String lower = name;
				lower.toLowerCase();
				if (!isDir && lower.endsWith(".m3u") && !isSystemOrHidden(name)) {
					String fullPath = "/playlists/" + name;
					File f = SD.open(fullPath, FILE_READ);
					bool found = false;
					if (f) {
						while (f.available()) {
							String line = f.readStringUntil('\n');
							line.trim();
							if (line == path) {
								found = true;
								break;
							}
						}
						f.close();
					}
					if (found) {
						String base = name;
						if (base.endsWith(".m3u")) {
							base = base.substring(0, base.length() - 4);
						}
						if (!first)
							server.sendContent(",");
						first = false;
						server.sendContent("\"" + jsonEscape(base) + "\"");
					}
				}
			}
			dir.close();
		}
	}

	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiTree() {
	String path = "/";
	if (server.hasArg("path"))
		path = server.arg("path");
	if (!isValidPath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}

	SdGuard guard;
	File dir = SD.open(normalizePath(path));
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		server.send(404, "application/json", "[]");
		return;
	}

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;

	iterateDirectory(dir, [&](const String &name, bool isDir, size_t sz) {
		if (!first)
			server.sendContent(",");
		first = false;
		if (isDir) {
			server.sendContent("{\"name\":\"" + jsonEscape(name) + "\",\"type\":\"dir\"}");
		} else {
			server.sendContent("{\"name\":\"" + jsonEscape(name) +
							   "\",\"type\":\"file\",\"size\":" + String(sz) + "}");
		}
	});

	dir.close();
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiFolderCreate() {
	heapLog("handleApiFolderCreate:enter");
	String path = "";
	if (server.hasArg("path")) {
		path = server.arg("path");
	} else if (server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			server.send(413, "application/json", "{\"error\":\"payload too large\"}");
			return;
		}
		path = extractJsonString(server.arg("plain"), "path");
	}
	if (path.length() == 0) {
		server.send(400, "application/json", "{\"error\":\"missing path\"}");
		return;
	}
	StorageResult res = createDirectory(path);
	heapLog("handleApiFolderCreate:after createDirectory");
	sendStorageResult(res);
}

inline void handleApiDelete() {
	heapLog("handleApiDelete:enter");
	if (!server.hasArg("path")) {
		server.send(400, "application/json", "{\"error\":\"missing path\"}");
		return;
	}
	String path = server.arg("path");
	StorageResult res = deleteStoragePath(path);
	heapLog("handleApiDelete:after deleteStoragePath");
	sendStorageResult(res);
}

inline void handleApiDownload() {
	if (!server.hasArg("path")) {
		server.send(400, "text/plain", "Missing path parameter");
		return;
	}
	String path = normalizePath(server.arg("path"));
	if (!isValidPath(path) || isProtectedPath(path)) {
		server.send(404, "text/plain", "File not found");
		return;
	}
	File file;
	size_t fileSize = 0;
	{
		SdGuard guard;
		if (!SD.exists(path)) {
			server.send(404, "text/plain", "File not found");
			return;
		}
		file = SD.open(path, FILE_READ);
		if (!file || file.isDirectory()) {
			if (file)
				file.close();
			server.send(400, "text/plain", "Cannot download directory");
			return;
		}
		fileSize = file.size();
	}

	String filename = path;
	int lastSlash = path.lastIndexOf('/');
	if (lastSlash != -1)
		filename = path.substring(lastSlash + 1);

	server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
	server.setContentLength(fileSize);
	server.send(200, "application/octet-stream", "");

	WiFiClient client = server.client();
	uint8_t buffer[2048];
	size_t bytesSent = 0;

	while (bytesSent < fileSize && client.connected()) {
		size_t toRead = sizeof(buffer);
		if (bytesSent + toRead > fileSize) {
			toRead = fileSize - bytesSent;
		}
		size_t actualRead = 0;
		{
			SdGuard guard;
			actualRead = file.read(buffer, toRead);
		}
		if (actualRead == 0)
			break;
		size_t written = client.write(buffer, actualRead);
		bytesSent += written;
		if (written != actualRead)
			break;
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	{
		SdGuard guard;
		file.close();
	}
}

inline void handleFileUploadProcess() {
	HTTPUpload &upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		heapLog("upload:START");
		uploadAborted = false;
		uploadError = "";
		uploadBytesWritten = 0;
		uploadFinalPath = "";
		uploadTempPath = "";

		if (!isStorageReady()) {
			uploadAborted = true;
			uploadError = "SD_UNAVAILABLE";
			return;
		}

		uint64_t freeBytes = getSdFreeBytes();
		if (freeBytes < MIN_SD_FREE_MARGIN) {
			uploadAborted = true;
			uploadError = "NO_SPACE";
			return;
		}

		String targetDir = "/";
		if (server.hasArg("path")) {
			String p = server.arg("path");
			if (isValidPath(p))
				targetDir = normalizePath(p);
		}
		String filename = upload.filename;
		int slashIndex = filename.lastIndexOf('/');
		if (slashIndex != -1)
			filename = filename.substring(slashIndex + 1);
		slashIndex = filename.lastIndexOf('\\');
		if (slashIndex != -1)
			filename = filename.substring(slashIndex + 1);

		if (!isSafeUploadFilename(filename)) {
			uploadAborted = true;
			uploadError = "INVALID_FILENAME";
			return;
		}

		uploadFinalPath = (targetDir == "/" ? "" : targetDir) + "/" + filename;
		uploadTempPath = uploadFinalPath + ".tmp";

		if (isProtectedPath(uploadFinalPath) || isAudioPathCurrentlyActive(uploadFinalPath)) {
			uploadAborted = true;
			uploadError = "PROTECTED_OR_ACTIVE";
			return;
		}

		SdGuard guard;
		if (SD.exists(uploadTempPath)) {
			SD.remove(uploadTempPath);
		}
		uploadFile = SD.open(uploadTempPath, FILE_WRITE);
		if (!uploadFile) {
			uploadAborted = true;
			uploadError = "TEMP_OPEN_FAILED";
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		static int writeCount = 0;
		if ((writeCount++ % 20) == 0) {
			heapLog("upload:WRITE tick");
		}
		if (!uploadAborted && uploadFile) {
			uploadBytesWritten += upload.currentSize;
			if (uploadBytesWritten > MAX_UPLOAD_SIZE) {
				uploadAborted = true;
				uploadError = "FILE_TOO_LARGE";
				SdGuard guard;
				uploadFile.close();
				if (uploadTempPath.length() > 0 && SD.exists(uploadTempPath)) {
					SD.remove(uploadTempPath);
				}
			} else {
				uint64_t freeBytes = getSdFreeBytes();
				if (freeBytes < MIN_SD_FREE_MARGIN) {
					uploadAborted = true;
					uploadError = "NO_SPACE";
					SdGuard guard;
					uploadFile.close();
					if (uploadTempPath.length() > 0 && SD.exists(uploadTempPath)) {
						SD.remove(uploadTempPath);
					}
					return;
				}
				SdGuard guard;
				size_t written = uploadFile.write(upload.buf, upload.currentSize);
				if (written != upload.currentSize) {
					heapLog("upload:WRITE_FAILED point");
					uploadAborted = true;
					uploadError = "WRITE_FAILED";
					uploadFile.close();
					if (uploadTempPath.length() > 0 && SD.exists(uploadTempPath)) {
						SD.remove(uploadTempPath);
					}
				}
			}
		}
	} else if (upload.status == UPLOAD_FILE_END) {
		heapLog("upload:END before commit");
		if (uploadFile) {
			SdGuard guard;
			uploadFile.close();
		}
		if (!uploadAborted) {
			StorageResult res =
				commitUploadFile(uploadTempPath, uploadFinalPath, uploadBytesWritten);
			heapLog("upload:END after commit");
			if (res != StorageResult::Ok) {
				uploadAborted = true;
				uploadError = "COMMIT_FAILED";
			}
		} else {
			SdGuard guard;
			if (uploadTempPath.length() > 0 && SD.exists(uploadTempPath)) {
				SD.remove(uploadTempPath);
			}
		}
	} else if (upload.status == UPLOAD_FILE_ABORTED) {
		if (uploadFile) {
			SdGuard guard;
			uploadFile.close();
		}
		SdGuard guard;
		if (uploadTempPath.length() > 0 && SD.exists(uploadTempPath)) {
			SD.remove(uploadTempPath);
		}
		uploadAborted = true;
		if (uploadError.length() == 0) {
			uploadError = "ABORTED";
		}
	}
}

inline void handleFileUploadResponse() {
	if (uploadAborted) {
		String err = uploadError.length() > 0 ? uploadError : "UPLOAD_FAILED";
		server.send(400, "application/json", "{\"status\":\"error\",\"error\":\"" + err + "\"}");
	} else {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	}
}

inline void handleApiPlaylistsGet() {
	if (!isStorageReady()) {
		server.send(503, "application/json", "[]");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;

	SdGuard guard;
	File dir = SD.open("/playlists");
	if (dir && dir.isDirectory()) {
		while (true) {
			File entry = dir.openNextFile();
			if (!entry)
				break;
			String name = entry.name();
			bool isDir = entry.isDirectory();
			entry.close();

			String lower = name;
			lower.toLowerCase();
			if (!isDir && lower.endsWith(".m3u") && !isSystemOrHidden(name)) {
				String fullPath = "/playlists/" + name;
				int count = 0;
				File f = SD.open(fullPath, FILE_READ);
				if (f) {
					while (f.available()) {
						String line = f.readStringUntil('\n');
						line.trim();
						if (line.length() > 0 && !line.startsWith("#")) {
							count++;
						}
					}
					f.close();
				}
				if (!first)
					server.sendContent(",");
				first = false;
				server.sendContent("{\"name\":\"" + jsonEscape(name) +
								   "\",\"trackCount\":" + String(count) + "}");
			}
		}
		dir.close();
	}

	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiPlaylistsPost() {
	heapLog("handleApiPlaylistsPost:enter");
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String body = server.arg("plain");
	String name = extractJsonString(body, "name");
	StorageResult res = createPlaylist(name);
	heapLog("handleApiPlaylistsPost:after createPlaylist");
	sendStorageResult(res);
}

inline void handleApiPlaylistTracksGet() {
	String uri = server.uri();
	int prefixLen = 15;
	int trackSuffix = uri.indexOf("/tracks");
	String id = "";
	if (trackSuffix != -1) {
		id = urldecode(uri.substring(prefixLen, trackSuffix));
	}
	if (!isValidPlaylistName(id)) {
		server.send(400, "application/json", "{\"error\":\"invalid id\"}");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	bool ok = iteratePlaylistTracks(
		id, [&](const String &name, const String &path, size_t sz, bool exists) {
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("{\"name\":\"" + jsonEscape(name) + "\",\"path\":\"" +
							   jsonEscape(path) + "\",\"size\":" + String(sz) +
							   ",\"exists\":" + (exists ? "true" : "false") + "}");
		});
	server.sendContent("]");
	server.sendContent("");
	if (!ok) {
		server.send(404, "application/json", "{\"error\":\"not found\"}");
	}
}

inline void handleApiPlaylistTracksUpdate() {
	heapLog("handleApiPlaylistTracksUpdate:enter");
	if (!server.hasArg("id")) {
		server.send(400, "application/json", "{\"error\":\"missing id\"}");
		return;
	}
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String id = urldecode(server.arg("id"));
	if (!isValidPlaylistName(id)) {
		server.send(400, "application/json", "{\"error\":\"invalid id\"}");
		return;
	}

	String body = server.arg("plain");
	String needle = "\"tracks\"";
	int kpos = body.indexOf(needle);
	if (kpos == -1) {
		server.send(400, "application/json", "{\"error\":\"missing tracks\"}");
		return;
	}
	int b1 = body.indexOf('[', kpos + needle.length());
	int b2 = body.lastIndexOf(']');
	if (b1 == -1 || b2 == -1 || b2 <= b1) {
		server.send(400, "application/json", "{\"error\":\"invalid tracks format\"}");
		return;
	}

	String destPath = getPlaylistPath(id);
	String tempPath = destPath + ".tmp";

	SdGuard guard;
	if (SD.exists(tempPath)) {
		SD.remove(tempPath);
	}
	File outF = SD.open(tempPath, FILE_WRITE);
	if (!outF) {
		server.send(500, "application/json", "{\"error\":\"open failed\"}");
		return;
	}

	int cur = b1 + 1;
	size_t count = 0;
	bool ok = true;
	while (cur < b2) {
		int q1 = body.indexOf('"', cur);
		if (q1 == -1 || q1 >= b2)
			break;
		String trackPath = "";
		bool escape = false;
		int endIdx = -1;
		for (int i = q1 + 1; i < b2; i++) {
			char c = body[i];
			if (escape) {
				trackPath += c;
				escape = false;
			} else if (c == '\\') {
				escape = true;
			} else if (c == '"') {
				endIdx = i;
				break;
			} else {
				trackPath += c;
			}
		}
		if (endIdx == -1)
			break;
		cur = endIdx + 1;

		String tp = normalizePath(trackPath);
		if (!isValidPath(tp) || !isMp3(tp) || !SD.exists(tp)) {
			ok = false;
			break;
		}
		if (++count > MAX_PLAYLIST_TRACKS) {
			ok = false;
			break;
		}
		outF.println(tp);
	}

	outF.close();

	if (!ok) {
		SD.remove(tempPath);
		server.send(400, "application/json", "{\"error\":\"invalid track list\"}");
		return;
	}

	StorageResult res = replaceFileSafelyUnderGuard(tempPath, destPath);
	heapLog("handleApiPlaylistTracksUpdate:after overwrite");
	sendStorageResult(res);
}

inline void handleApiPlaylistTracksAppend() {
	if (!server.hasArg("id")) {
		server.send(400, "application/json", "{\"error\":\"missing id\"}");
		return;
	}
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String id = urldecode(server.arg("id"));
	String body = server.arg("plain");
	String path = extractJsonString(body, "path");
	StorageResult res = appendPlaylistTrack(id, path);
	sendStorageResult(res);
}

inline void handleApiPlayerToggle() {
	if (isBluetoothActive()) {
		server.send(403, "application/json", "{\"error\":\"device in bluetooth mode\"}");
		return;
	}
	String path = "";
	if (server.hasArg("path")) {
		path = server.arg("path");
	} else if (server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			server.send(413, "application/json", "{\"error\":\"payload too large\"}");
			return;
		}
		path = extractJsonString(server.arg("plain"), "path");
	}
	if (path.length() == 0 || !isValidPath(path) || !isMp3(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	if (requestAudioToggle(path)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(500, "application/json", "{\"error\":\"playback request failed\"}");
	}
}

inline void handleApiPlayerStatus() {
	AudioStateSnapshot snap = getAudioSnapshot();
	String state = "stopped";
	if (isBluetoothActive()) {
		state = isBluetoothConnected() ? "bluetooth_connected" : "bluetooth_pairing";
	} else if (snap.valid && snap.isPlaying) {
		state = snap.isPaused ? "paused" : "playing";
	}
	server.send(200, "application/json",
				"{\"status\":\"ok\",\"state\":\"" + state + "\",\"path\":\"" +
					jsonEscape(String(snap.path)) + "\"}");
}

inline void handleDynamicPlaylists() {
	String uri = server.uri();
	HTTPMethod method = server.method();
	if (uri.startsWith("/api/playlists/") && uri.endsWith("/tracks") && method == HTTP_GET) {
		handleApiPlaylistTracksGet();
		return;
	}
	if (uri.startsWith("/api/playlists/")) {
		String id = urldecode(uri.substring(15));
		if (!isValidPlaylistName(id)) {
			server.send(400, "application/json", "{\"error\":\"invalid id\"}");
			return;
		}
		if (method == HTTP_PUT) {
			if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
				server.send(413, "application/json", "{\"error\":\"payload too large\"}");
				return;
			}
			String body = server.arg("plain");
			String newName = extractJsonString(body, "newName");
			if (!isValidPlaylistName(newName)) {
				server.send(400, "application/json", "{\"error\":\"invalid newName\"}");
				return;
			}
			StorageResult res = renamePlaylist(id, newName);
			if (res == StorageResult::Ok) {
				if (!cascadeRenamePlaylist(id, newName)) {
					renamePlaylist(newName, id);
					server.send(500, "application/json", "{\"error\":\"mapping cascade failed\"}");
					return;
				}
				server.send(200, "application/json", "{\"status\":\"ok\"}");
			} else {
				sendStorageResult(res);
			}
			return;
		}
		if (method == HTTP_DELETE) {
			StorageResult res = deletePlaylist(id);
			if (res == StorageResult::Ok) {
				cascadeDeletePlaylist(id);
				server.send(200, "application/json", "{\"status\":\"ok\"}");
			} else {
				sendStorageResult(res);
			}
			return;
		}
	}
	server.send(404, "application/json", "{\"error\":\"not found\"}");
}

inline void initServer() {
	server.on("/", HTTP_GET, handleRoot);
	server.on("/now", HTTP_GET, handleNow);
	server.on("/api/mp3", HTTP_GET, handleApiMp3);
	server.on("/api/track/playlists", HTTP_GET, handleApiTrackPlaylists);
	server.on("/api/tree", HTTP_GET, handleApiTree);
	server.on("/api/folders", HTTP_POST, handleApiFolderCreate);
	server.on("/api/delete", HTTP_DELETE, handleApiDelete);
	server.on("/api/download", HTTP_GET, handleApiDownload);
	server.on("/api/upload", HTTP_POST, handleFileUploadResponse, handleFileUploadProcess);

	server.on("/api/discs", HTTP_GET, handleApiDiscsGet);
	server.on("/api/discs/map", HTTP_POST, handleApiDiscsMap);
	server.on("/api/discs", HTTP_DELETE, handleApiDiscsDelete);

	server.on("/api/player/toggle", HTTP_POST, handleApiPlayerToggle);
	server.on("/api/player/status", HTTP_GET, handleApiPlayerStatus);

	server.on("/api/playlists", HTTP_GET, handleApiPlaylistsGet);
	server.on("/api/playlists", HTTP_POST, handleApiPlaylistsPost);
	server.on("/api/playlists/tracks", HTTP_PUT, handleApiPlaylistTracksUpdate);
	server.on("/api/playlists/tracks", HTTP_POST, handleApiPlaylistTracksAppend);

	server.onNotFound(handleDynamicPlaylists);
}

inline void handleServer() { server.handleClient(); }
