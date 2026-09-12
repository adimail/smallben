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
#include <vector>

WebServer server(HTTP_PORT);

static File uploadFile;
static bool uploadAborted = false;
static String uploadError = "";
static size_t uploadBytesWritten = 0;
static String uploadFinalPath = "";
static String uploadTempPath = "";

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

inline std::vector<String> extractJsonArray(const String &json, const String &key) {
	std::vector<String> res;
	String needle = "\"" + key + "\"";
	int kpos = json.indexOf(needle);
	if (kpos == -1)
		return res;
	int b1 = json.indexOf('[', kpos + needle.length());
	int b2 = json.lastIndexOf(']');
	if (b1 == -1 || b2 == -1 || b2 <= b1)
		return res;
	String arr = json.substring(b1 + 1, b2);
	int cur = 0;
	while (cur < (int)arr.length()) {
		int q1 = arr.indexOf('"', cur);
		if (q1 == -1)
			break;
		String item = "";
		bool escape = false;
		int endIdx = -1;
		for (int i = q1 + 1; i < (int)arr.length(); i++) {
			char c = arr[i];
			if (escape) {
				item += c;
				escape = false;
			} else if (c == '\\') {
				escape = true;
			} else if (c == '"') {
				endIdx = i;
				break;
			} else {
				item += c;
			}
		}
		if (endIdx != -1) {
			res.push_back(item);
			cur = endIdx + 1;
		} else {
			break;
		}
	}
	return res;
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
	json += "\"ready\":" + String(isSdReady() ? "true" : "false") + ",";
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
	std::vector<DiscMapping> list = getDiscMappingsSnapshot();
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	for (const auto &item : list) {
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"uid\":\"" + jsonEscape(item.uid) + "\",\"type\":\"" +
						   jsonEscape(item.type) + "\",\"target\":\"" + jsonEscape(item.target) +
						   "\",\"label\":\"" + jsonEscape(item.label) + "\"}");
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiDiscsMap() {
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String body = server.arg("plain");
	String uid = extractJsonString(body, "uid");
	String type = extractJsonString(body, "type");
	String target = extractJsonString(body, "target");
	String label = extractJsonString(body, "label");
	if (mapDisc(uid, type, target, label)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(
			400, "application/json",
			"{\"error\":\"invalid parameters, table full, target invalid, or write failed\"}");
	}
}

inline void handleApiDiscsDelete() {
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
	UnmapResult res = unmapDisc(uid);
	if (res == UnmapResult::Ok) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else if (res == UnmapResult::NotFound) {
		server.send(404, "application/json", "{\"error\":\"disc not found\"}");
	} else {
		server.send(500, "application/json",
					"{\"error\":\"SD storage error - disc was not removed\"}");
	}
}

inline void handleApiMp3() {
	if (!isSdReady()) {
		server.send(500, "application/json", "[]");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	std::vector<String> dirs;
	dirs.push_back("/");

	while (!dirs.empty()) {
		String currentDir = dirs.back();
		dirs.pop_back();

		std::vector<String> subDirs;
		{
			SdGuard guard;
			File dir = SD.open(currentDir);
			if (dir && dir.isDirectory()) {
				while (true) {
					File entry = dir.openNextFile();
					if (!entry)
						break;
					String name = entry.name();
					if (!isJunk(name)) {
						String fullPath = (currentDir == "/" ? "" : currentDir) + "/" + name;
						if (entry.isDirectory()) {
							subDirs.push_back(fullPath);
						} else if (isMp3(name)) {
							size_t sz = entry.size();
							if (!first)
								server.sendContent(",");
							first = false;
							server.sendContent("{\"name\":\"" + jsonEscape(name) +
											   "\",\"path\":\"" + jsonEscape(fullPath) +
											   "\",\"size\":" + String(sz) + "}");
						}
					}
					entry.close();
				}
				dir.close();
			}
		}
		for (const auto &sd : subDirs) {
			dirs.push_back(sd);
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiTrackPlaylists() {
	if (!server.hasArg("path")) {
		server.send(400, "application/json", "{\"error\":\"missing path\"}");
		return;
	}
	String path = server.arg("path");
	if (!isSafePath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;

	if (isSdReady()) {
		std::vector<String> playlistNames;
		{
			SdGuard guard;
			File dir = SD.open("/playlists");
			if (dir && dir.isDirectory()) {
				while (true) {
					File entry = dir.openNextFile();
					if (!entry)
						break;
					String name = entry.name();
					String lower = name;
					lower.toLowerCase();
					if (!entry.isDirectory() && lower.endsWith(".m3u") && !name.startsWith(".") &&
						!name.startsWith("._")) {
						playlistNames.push_back(name);
					}
					entry.close();
				}
				dir.close();
			}
		}

		for (const String &pName : playlistNames) {
			String fullPath = "/playlists/" + pName;
			bool found = false;
			{
				SdGuard guard;
				File f = SD.open(fullPath, FILE_READ);
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
			}
			if (found) {
				String base = pName;
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

	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiTree() {
	String path = "/";
	if (server.hasArg("path"))
		path = server.arg("path");
	if (!isSafePath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	struct TreeNode {
		String name;
		bool isDir;
		size_t size;
	};
	std::vector<TreeNode> nodes;
	{
		SdGuard guard;
		File dir = SD.open(path);
		if (!dir || !dir.isDirectory()) {
			if (dir)
				dir.close();
			server.send(404, "application/json", "[]");
			return;
		}
		iterateDirectory(dir, [&](const String &name, bool isDir, size_t sz) {
			if (nodes.size() < 256) {
				nodes.push_back({name, isDir, sz});
			}
		});
		dir.close();
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	for (const auto &n : nodes) {
		if (!first)
			server.sendContent(",");
		first = false;
		if (n.isDir) {
			server.sendContent("{\"name\":\"" + jsonEscape(n.name) + "\",\"type\":\"dir\"}");
		} else {
			server.sendContent("{\"name\":\"" + jsonEscape(n.name) +
							   "\",\"type\":\"file\",\"size\":" + String(n.size) + "}");
		}
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiDelete() {
	if (!server.hasArg("path")) {
		server.send(400, "application/json", "{\"error\":\"missing path\"}");
		return;
	}
	String path = server.arg("path");
	if (deleteStoragePath(path)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(403, "application/json",
					"{\"error\":\"delete failed, protected, or currently active\"}");
	}
}

inline void handleApiDownload() {
	if (!server.hasArg("path")) {
		server.send(400, "text/plain", "Missing path parameter");
		return;
	}
	String path = server.arg("path");
	if (!isSafePath(path) || isProtectedStoragePath(path)) {
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
		uploadAborted = false;
		uploadError = "";
		uploadBytesWritten = 0;
		uploadFinalPath = "";
		uploadTempPath = "";

		uint64_t freeBytes = getSdFreeBytes();
		if (freeBytes < MIN_SD_FREE_MARGIN) {
			uploadAborted = true;
			uploadError = "NO_SPACE";
			return;
		}

		String targetDir = "/";
		if (server.hasArg("path")) {
			String p = server.arg("path");
			if (isSafePath(p))
				targetDir = p;
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
			uploadError = "INVALID_FILE";
			return;
		}

		uploadFinalPath = (targetDir == "/" ? "" : targetDir) + "/" + filename;
		uploadTempPath = uploadFinalPath + ".uploading";

		SdGuard guard;
		if (!canMutateStoragePathUnderGuard(uploadFinalPath)) {
			uploadAborted = true;
			uploadError = "INVALID_PATH";
			return;
		}

		if (SD.exists(uploadTempPath)) {
			SD.remove(uploadTempPath);
		}
		uploadFile = SD.open(uploadTempPath, FILE_WRITE);
		if (!uploadFile) {
			uploadAborted = true;
			uploadError = "TEMP_OPEN_FAILED";
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
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
		if (uploadFile) {
			SdGuard guard;
			uploadFile.close();
		}
		if (!uploadAborted) {
			SdGuard guard;
			if (!SD.exists(uploadTempPath)) {
				uploadAborted = true;
				uploadError = "TEMP_MISSING";
				return;
			}
			File tempF = SD.open(uploadTempPath, FILE_READ);
			size_t tempSize = tempF ? tempF.size() : 0;
			if (tempF) {
				tempF.close();
			}
			if (tempSize != uploadBytesWritten) {
				uploadAborted = true;
				uploadError = "VERIFY_FAILED";
				SD.remove(uploadTempPath);
				return;
			}
			if (!canMutateStoragePathUnderGuard(uploadFinalPath)) {
				uploadAborted = true;
				uploadError = "CANNOT_MUTATE";
				SD.remove(uploadTempPath);
				return;
			}
			if (SD.exists(uploadFinalPath)) {
				SD.remove(uploadFinalPath);
			}
			if (!SD.rename(uploadTempPath, uploadFinalPath)) {
				uploadAborted = true;
				uploadError = "RENAME_FAILED";
				SD.remove(uploadTempPath);
				return;
			}
			File finalF = SD.open(uploadFinalPath, FILE_READ);
			size_t finalSize = finalF ? finalF.size() : 0;
			if (finalF) {
				finalF.close();
			}
			if (finalSize != uploadBytesWritten) {
				uploadAborted = true;
				uploadError = "VERIFY_FINAL_FAILED";
				SD.remove(uploadFinalPath);
				return;
			}
			refreshSdStatsDirect();
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
	if (server.hasArg("path") && uploadFile.name() == nullptr && uploadBytesWritten == 0 &&
		!uploadAborted) {
		String dirPath = server.arg("path");
		if (isSafePath(dirPath) && canMutateStoragePathUnderGuard(dirPath)) {
			SdGuard guard;
			if (!SD.exists(dirPath)) {
				SD.mkdir(dirPath);
				server.send(200, "application/json", "{\"status\":\"ok\"}");
				return;
			}
		}
	}
	if (uploadAborted) {
		String err = uploadError.length() > 0 ? uploadError : "UPLOAD_FAILED";
		server.send(400, "application/json", "{\"status\":\"error\",\"error\":\"" + err + "\"}");
	} else {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	}
}

inline void handleApiPlaylistsGet() {
	if (!isSdReady()) {
		server.send(500, "application/json", "[]");
		return;
	}
	std::vector<String> playlistNames;
	{
		SdGuard guard;
		File dir = SD.open("/playlists");
		if (dir && dir.isDirectory()) {
			while (true) {
				File entry = dir.openNextFile();
				if (!entry)
					break;
				String name = entry.name();
				String lower = name;
				lower.toLowerCase();
				if (!entry.isDirectory() && lower.endsWith(".m3u") && !name.startsWith(".") &&
					!name.startsWith("._")) {
					playlistNames.push_back(name);
				}
				entry.close();
			}
			dir.close();
		}
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	for (const String &name : playlistNames) {
		String fullPath = "/playlists/" + name;
		int count = 0;
		{
			SdGuard guard;
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
		}
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"name\":\"" + jsonEscape(name) +
						   "\",\"trackCount\":" + String(count) + "}");
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiPlaylistsPost() {
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String body = server.arg("plain");
	String name = extractJsonString(body, "name");
	if (!isSafePlaylistName(name)) {
		server.send(400, "application/json", "{\"error\":\"invalid name\"}");
		return;
	}
	if (createPlaylist(name)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(409, "application/json", "{\"error\":\"already exists or write failed\"}");
	}
}

inline void handleApiPlaylistTracksGet() {
	String uri = server.uri();
	int prefixLen = 15;
	int trackSuffix = uri.indexOf("/tracks");
	String id = "";
	if (trackSuffix != -1) {
		id = urldecode(uri.substring(prefixLen, trackSuffix));
	}
	if (!isSafePlaylistName(id)) {
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
	if (!server.hasArg("id")) {
		server.send(400, "application/json", "{\"error\":\"missing id\"}");
		return;
	}
	if (server.hasArg("plain") && server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		server.send(413, "application/json", "{\"error\":\"payload too large\"}");
		return;
	}
	String id = urldecode(server.arg("id"));
	if (!isSafePlaylistName(id)) {
		server.send(400, "application/json", "{\"error\":\"invalid id\"}");
		return;
	}
	String body = server.arg("plain");
	std::vector<String> tracks = extractJsonArray(body, "tracks");
	if (overwritePlaylistTracks(id, tracks)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(400, "application/json",
					"{\"error\":\"write failed, invalid track, or track missing\"}");
	}
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
	if (!isSafePlaylistName(id)) {
		server.send(400, "application/json", "{\"error\":\"invalid id\"}");
		return;
	}
	String body = server.arg("plain");
	String path = extractJsonString(body, "path");
	if (!isSafePath(path) || !isMp3(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	if (appendPlaylistTrack(id, path)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(500, "application/json",
					"{\"error\":\"append failed, track missing, or playlist full\"}");
	}
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
	if (path.length() == 0 || !isSafePath(path) || !isMp3(path)) {
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
		if (!isSafePlaylistName(id)) {
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
			if (!isSafePlaylistName(newName)) {
				server.send(400, "application/json", "{\"error\":\"invalid newName\"}");
				return;
			}
			if (renamePlaylist(id, newName)) {
				if (!cascadeRenamePlaylist(id, newName)) {
					renamePlaylist(newName, id);
					server.send(500, "application/json", "{\"error\":\"mapping cascade failed\"}");
					return;
				}
				server.send(200, "application/json", "{\"status\":\"ok\"}");
			} else {
				server.send(500, "application/json", "{\"error\":\"rename failed\"}");
			}
			return;
		}
		if (method == HTTP_DELETE) {
			if (deletePlaylist(id)) {
				cascadeDeletePlaylist(id);
				server.send(200, "application/json", "{\"status\":\"ok\"}");
			} else {
				server.send(500, "application/json", "{\"error\":\"delete failed\"}");
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
