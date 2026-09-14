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
#include <cJSON.h>

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

inline void sendJson(int code, const String &body) { server.send(code, "application/json", body); }

inline void sendJsonOk() { sendJson(200, "{\"status\":\"ok\"}"); }

inline void sendJsonError(int code, const String &msg) {
	triggerErrorAudio();
	sendJson(code, "{\"error\":\"" + jsonEscape(msg) + "\"}");
}

inline void sendStorageResult(StorageResult res, const String &successMsg = "{\"status\":\"ok\"}") {
	switch (res) {
	case StorageResult::Ok:
		sendJson(200, successMsg);
		break;
	case StorageResult::NotFound:
		sendJsonError(404, "not found");
		break;
	case StorageResult::AlreadyExists:
		sendJsonError(409, "already exists");
		break;
	case StorageResult::InvalidPath:
		sendJsonError(400, "invalid path or name");
		break;
	case StorageResult::Protected:
		sendJsonError(403, "path is protected");
		break;
	case StorageResult::Active:
		sendJsonError(409, "item is currently active in playback");
		break;
	case StorageResult::NoSpace:
		sendJsonError(507, "storage full");
		break;
	case StorageResult::StorageUnavailable:
		sendJsonError(503, "SD card unavailable");
		break;
	default:
		sendJsonError(500, "storage operation failed");
		break;
	}
}

inline void handleRoot() { streamDashboard(server); }

inline void handleNow() {
	AudioState audio = getAudioState();
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
	} else if (audio.isPlaying) {
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

	sendJson(200, json);
}

inline void handleApiDiscsGet() {
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	sendJson(200, "");
	server.sendContent("[");
	bool first = true;
	File f;
	{
		SdGuard guard;
		if (isStorageReady() && SD.exists("/discs.csv")) {
			f = SD.open("/discs.csv", FILE_READ);
		}
	}
	if (f) {
		while (true) {
			DiscMappingRecord rec;
			bool haveRec = false;
			{
				SdGuard guard;
				while (f.available()) {
					String line = f.readStringUntil('\n');
					line.trim();
					if (line.length() == 0)
						continue;
					if (parseMappingLine(line, rec)) {
						haveRec = true;
						break;
					}
				}
			}
			if (!haveRec)
				break;
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("{\"uid\":\"" + jsonEscape(rec.uid) + "\",\"type\":\"" +
							   jsonEscape(rec.type) + "\",\"target\":\"" + jsonEscape(rec.target) +
							   "\",\"label\":\"" + jsonEscape(rec.label) + "\"}");
		}
		{
			SdGuard guard;
			f.close();
		}
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiDiscsMap() {
	heapLog("handleApiDiscsMap:enter");
	String uid = server.arg("uid");
	String type = server.arg("type");
	String target = server.arg("target");
	String label = server.arg("label");

	if (server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			sendJsonError(413, "payload too large");
			return;
		}
		cJSON *root = cJSON_Parse(server.arg("plain").c_str());
		if (root) {
			cJSON *item;
			if ((item = cJSON_GetObjectItem(root, "uid")) && cJSON_IsString(item) &&
				item->valuestring)
				uid = item->valuestring;
			if ((item = cJSON_GetObjectItem(root, "type")) && cJSON_IsString(item) &&
				item->valuestring)
				type = item->valuestring;
			if ((item = cJSON_GetObjectItem(root, "target")) && cJSON_IsString(item) &&
				item->valuestring)
				target = item->valuestring;
			if ((item = cJSON_GetObjectItem(root, "label")) && cJSON_IsString(item) &&
				item->valuestring)
				label = item->valuestring;
			cJSON_Delete(root);
		}
	}

	StorageResult res = mapDisc(uid, type, target, label);
	heapLog("handleApiDiscsMap:after mapDisc");
	sendStorageResult(res);
}

inline void handleApiDiscsDelete() {
	heapLog("handleApiDiscsDelete:enter");
	String uid = server.arg("uid");
	if (uid.length() == 0 && server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			sendJsonError(413, "payload too large");
			return;
		}
		cJSON *root = cJSON_Parse(server.arg("plain").c_str());
		if (root) {
			cJSON *item = cJSON_GetObjectItem(root, "uid");
			if (cJSON_IsString(item) && item->valuestring) {
				uid = item->valuestring;
			}
			cJSON_Delete(root);
		}
	}
	StorageResult res = unmapDisc(uid);
	heapLog("handleApiDiscsDelete:after unmapDisc");
	sendStorageResult(res);
}

inline void streamMp3Entries(const String &dirPath, bool &first) {
	File dir;
	{
		SdGuard guard;
		dir = SD.open(dirPath);
		if (!dir || !dir.isDirectory()) {
			if (dir)
				dir.close();
			return;
		}
	}

	while (true) {
		String name = "";
		bool isDir = false;
		size_t sz = 0;
		bool hasEntry = false;
		{
			SdGuard guard;
			File entry = dir.openNextFile();
			if (entry) {
				name = entry.name();
				isDir = entry.isDirectory();
				sz = entry.size();
				entry.close();
				hasEntry = true;
			}
		}
		if (!hasEntry)
			break;

		String lowerName = name;
		lowerName.toLowerCase();
		if (isSystemOrHidden(name) || lowerName == "smallben" || lowerName == "p" ||
			lowerName == "playlists")
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
	{
		SdGuard guard;
		dir.close();
	}
}

inline void handleApiMp3() {
	heapLog("handleApiMp3:enter");
	if (!isStorageReady()) {
		sendJson(503, "[]");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	sendJson(200, "");
	server.sendContent("[");
	bool first = true;
	streamMp3Entries("/", first);
	server.sendContent("]");
	server.sendContent("");
	heapLog("handleApiMp3:done");
}

inline void handleApiTrackPlaylists() {
	if (!server.hasArg("path")) {
		sendJsonError(400, "missing path");
		return;
	}
	String path = server.arg("path");
	if (!isValidPath(path)) {
		sendJsonError(400, "invalid path");
		return;
	}

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	sendJson(200, "");
	server.sendContent("[");
	bool first = true;

	if (isStorageReady()) {
		File dir;
		{
			SdGuard guard;
			dir = SD.open("/p");
		}
		if (dir && dir.isDirectory()) {
			while (true) {
				String matchedPlaylist = "";
				bool hasMatch = false;
				{
					SdGuard guard;
					while (true) {
						File entry = dir.openNextFile();
						if (!entry)
							break;
						String name = entry.name();
						bool isDir = entry.isDirectory();
						entry.close();

						String lower = name;
						lower.toLowerCase();
						if (!isDir && lower.endsWith(".m3u") && !name.startsWith(".")) {
							String fullPath = "/p/" + name;
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
								matchedPlaylist = name.substring(0, name.length() - 4);
								hasMatch = true;
								break;
							}
						}
					}
				}
				if (!hasMatch)
					break;
				if (!first)
					server.sendContent(",");
				first = false;
				server.sendContent("\"" + jsonEscape(matchedPlaylist) + "\"");
			}
			{
				SdGuard guard;
				dir.close();
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
	if (!isValidPath(path)) {
		sendJsonError(400, "invalid path");
		return;
	}

	File dir;
	{
		SdGuard guard;
		dir = SD.open(normalizePath(path));
		if (!dir || !dir.isDirectory()) {
			if (dir)
				dir.close();
			sendJson(404, "[]");
			return;
		}
	}

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	sendJson(200, "");
	server.sendContent("[");
	bool first = true;

	while (true) {
		String name = "";
		bool isDir = false;
		size_t sz = 0;
		bool hasEntry = false;
		{
			SdGuard guard;
			while (true) {
				File entry = dir.openNextFile();
				if (!entry)
					break;
				String ename = entry.name();
				bool edir = entry.isDirectory();
				size_t esz = entry.size();
				entry.close();
				if (isSystemOrHidden(ename))
					continue;
				name = ename;
				isDir = edir;
				sz = esz;
				hasEntry = true;
				break;
			}
		}
		if (!hasEntry)
			break;
		if (!first)
			server.sendContent(",");
		first = false;
		if (isDir) {
			server.sendContent("{\"name\":\"" + jsonEscape(name) + "\",\"type\":\"dir\"}");
		} else {
			server.sendContent("{\"name\":\"" + jsonEscape(name) +
							   "\",\"type\":\"file\",\"size\":" + String(sz) + "}");
		}
	}

	{
		SdGuard guard;
		dir.close();
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiFolderCreate() {
	heapLog("handleApiFolderCreate:enter");
	String path = server.arg("path");
	if (path.length() == 0 && server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			sendJsonError(413, "payload too large");
			return;
		}
		cJSON *root = cJSON_Parse(server.arg("plain").c_str());
		if (root) {
			cJSON *item = cJSON_GetObjectItem(root, "path");
			if (cJSON_IsString(item) && item->valuestring) {
				path = item->valuestring;
			}
			cJSON_Delete(root);
		}
	}
	if (path.length() == 0) {
		sendJsonError(400, "missing path");
		return;
	}
	StorageResult res = createDirectory(path);
	heapLog("handleApiFolderCreate:after createDirectory");
	sendStorageResult(res);
}

inline void handleApiDelete() {
	heapLog("handleApiDelete:enter");
	if (!server.hasArg("path")) {
		sendJsonError(400, "missing path");
		return;
	}
	String path = server.arg("path");
	StorageResult res = deleteStoragePath(path);
	heapLog("handleApiDelete:after deleteStoragePath");
	sendStorageResult(res);
}

inline void handleApiDownload() {
	if (!server.hasArg("path")) {
		triggerErrorAudio();
		server.send(400, "text/plain", "Missing path parameter");
		return;
	}
	String path = normalizePath(server.arg("path"));
	if (!isValidPath(path) || isProtectedPath(path)) {
		triggerErrorAudio();
		server.send(404, "text/plain", "File not found");
		return;
	}
	File file;
	size_t fileSize = 0;
	{
		SdGuard guard;
		if (!SD.exists(path)) {
			triggerErrorAudio();
			server.send(404, "text/plain", "File not found");
			return;
		}
		file = SD.open(path, FILE_READ);
		if (!file || file.isDirectory()) {
			if (file)
				file.close();
			triggerErrorAudio();
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

		if (!isValidName(filename) || isSystemOrHidden(filename)) {
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
		triggerErrorAudio();
		String err = uploadError.length() > 0 ? uploadError : "UPLOAD_FAILED";
		sendJson(400, "{\"status\":\"error\",\"error\":\"" + jsonEscape(err) + "\"}");
	} else {
		sendJsonOk();
	}
}

inline void handleApiPlaylistsGet() {
	if (!isStorageReady()) {
		sendJson(503, "[]");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	sendJson(200, "");
	server.sendContent("[");
	bool first = true;

	File dir;
	{
		SdGuard guard;
		dir = SD.open("/p");
	}
	if (dir && dir.isDirectory()) {
		while (true) {
			String baseName = "";
			int trackCount = 0;
			bool hasPlaylist = false;
			{
				SdGuard guard;
				while (true) {
					File entry = dir.openNextFile();
					if (!entry)
						break;
					String name = entry.name();
					bool isDir = entry.isDirectory();
					entry.close();

					String lower = name;
					lower.toLowerCase();
					if (!isDir && lower.endsWith(".m3u") && !name.startsWith(".")) {
						String fullPath = "/p/" + name;
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
						baseName = name.substring(0, name.length() - 4);
						trackCount = count;
						hasPlaylist = true;
						break;
					}
				}
			}
			if (!hasPlaylist)
				break;
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("{\"name\":\"" + jsonEscape(baseName) +
							   "\",\"trackCount\":" + String(trackCount) + "}");
		}
		{
			SdGuard guard;
			dir.close();
		}
	}

	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiPlaylistsPost() {
	heapLog("handleApiPlaylistsPost:enter");
	String name = server.arg("name");
	if (name.length() == 0 && server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			sendJsonError(413, "payload too large");
			return;
		}
		cJSON *root = cJSON_Parse(server.arg("plain").c_str());
		if (root) {
			cJSON *item = cJSON_GetObjectItem(root, "name");
			if (cJSON_IsString(item) && item->valuestring) {
				name = item->valuestring;
			}
			cJSON_Delete(root);
		}
	}
	StorageResult res = createPlaylist(name);
	heapLog("handleApiPlaylistsPost:after createPlaylist");
	sendStorageResult(res);
}

inline void handleApiPlaylistTracksGet() {
	String uri = server.uri();
	const String prefix = "/api/p/";
	int trackSuffix = uri.indexOf("/tracks");
	String id = "";
	if (trackSuffix != -1 && trackSuffix > (int)prefix.length()) {
		id = urldecode(uri.substring(prefix.length(), trackSuffix));
	}
	if (!isValidPlaylistName(id)) {
		sendJsonError(400, "invalid id");
		return;
	}

	String path = getPlaylistPath(id);
	File f;
	{
		SdGuard guard;
		if (!isStorageReady() || !SD.exists(path)) {
			sendJsonError(404, "not found");
			return;
		}
		f = SD.open(path, FILE_READ);
		if (!f) {
			sendJsonError(404, "not found");
			return;
		}
	}

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	sendJson(200, "");
	server.sendContent("[");
	bool first = true;

	while (true) {
		String trackName = "";
		String trackPath = "";
		size_t sz = 0;
		bool exists = false;
		bool hasTrack = false;
		{
			SdGuard guard;
			while (f.available()) {
				String line = f.readStringUntil('\n');
				line.trim();
				if (line.length() > 0 && !line.startsWith("#")) {
					trackPath = line;
					int lastSlash = trackPath.lastIndexOf('/');
					trackName = (lastSlash != -1) ? trackPath.substring(lastSlash + 1) : trackPath;
					if (SD.exists(trackPath)) {
						File tf = SD.open(trackPath, FILE_READ);
						if (tf) {
							sz = tf.size();
							exists = true;
							tf.close();
						}
					}
					hasTrack = true;
					break;
				}
			}
		}
		if (!hasTrack)
			break;
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"name\":\"" + jsonEscape(trackName) + "\",\"path\":\"" +
						   jsonEscape(trackPath) + "\",\"size\":" + String(sz) +
						   ",\"exists\":" + (exists ? "true" : "false") + "}");
	}

	{
		SdGuard guard;
		f.close();
	}

	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiPlaylistTracksUpdate() {
	heapLog("handleApiPlaylistTracksUpdate:enter");
	String id = urldecode(server.arg("id"));
	if (!isValidPlaylistName(id)) {
		sendJsonError(400, "invalid id");
		return;
	}
	if (!server.hasArg("plain")) {
		sendJsonError(400, "missing tracks");
		return;
	}
	if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
		sendJsonError(413, "payload too large");
		return;
	}

	cJSON *root = cJSON_Parse(server.arg("plain").c_str());
	if (!root) {
		sendJsonError(400, "invalid json");
		return;
	}
	cJSON *tracks = cJSON_GetObjectItem(root, "tracks");
	if (!cJSON_IsArray(tracks)) {
		cJSON_Delete(root);
		sendJsonError(400, "missing tracks");
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
		cJSON_Delete(root);
		sendJsonError(500, "open failed");
		return;
	}

	bool ok = true;
	size_t count = 0;
	cJSON *elem = nullptr;
	cJSON_ArrayForEach(elem, tracks) {
		if (!cJSON_IsString(elem) || !elem->valuestring) {
			ok = false;
			break;
		}
		String tp = normalizePath(String(elem->valuestring));
		if (!isValidPath(tp) || !isMp3(tp)) {
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
	cJSON_Delete(root);

	if (!ok) {
		SD.remove(tempPath);
		sendJsonError(400, "invalid track list");
		return;
	}

	StorageResult res = replaceFileSafelyUnderLock(tempPath, destPath);
	heapLog("handleApiPlaylistTracksUpdate:after overwrite");
	sendStorageResult(res);
}

inline void handleApiPlaylistTracksAppend() {
	String id = urldecode(server.arg("id"));
	String path = server.arg("path");
	if (path.length() == 0 && server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			sendJsonError(413, "payload too large");
			return;
		}
		cJSON *root = cJSON_Parse(server.arg("plain").c_str());
		if (root) {
			cJSON *item = cJSON_GetObjectItem(root, "path");
			if (cJSON_IsString(item) && item->valuestring) {
				path = item->valuestring;
			}
			cJSON_Delete(root);
		}
	}
	StorageResult res = appendPlaylistTrack(id, path);
	sendStorageResult(res);
}

inline void handleApiPlayerToggle() {
	if (isBluetoothActive()) {
		sendJsonError(403, "device in bluetooth mode");
		return;
	}
	String path = server.arg("path");
	if (path.length() == 0 && server.hasArg("plain")) {
		if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
			sendJsonError(413, "payload too large");
			return;
		}
		cJSON *root = cJSON_Parse(server.arg("plain").c_str());
		if (root) {
			cJSON *item = cJSON_GetObjectItem(root, "path");
			if (cJSON_IsString(item) && item->valuestring) {
				path = item->valuestring;
			}
			cJSON_Delete(root);
		}
	}
	if (path.length() == 0 || !isValidPath(path) || !isMp3(path)) {
		sendJsonError(400, "invalid path");
		return;
	}
	if (requestAudioToggle(path)) {
		sendJsonOk();
	} else {
		sendJsonError(500, "playback request failed");
	}
}

inline void handleApiPlayerStatus() {
	AudioState snap = getAudioState();
	String state = "stopped";
	if (isBluetoothActive()) {
		state = isBluetoothConnected() ? "bluetooth_connected" : "bluetooth_pairing";
	} else if (snap.isPlaying) {
		state = snap.isPaused ? "paused" : "playing";
	}
	sendJson(200, "{\"status\":\"ok\",\"state\":\"" + state + "\",\"path\":\"" +
					  jsonEscape(String(snap.path)) + "\"}");
}

inline void handleDynamicPlaylists() {
	String uri = server.uri();
	HTTPMethod method = server.method();
	if (uri.startsWith("/api/p/") && uri.endsWith("/tracks") && method == HTTP_GET) {
		handleApiPlaylistTracksGet();
		return;
	}
	const String prefix = "/api/p/";
	if (uri.startsWith(prefix)) {
		String id = urldecode(uri.substring(prefix.length()));
		if (!isValidPlaylistName(id)) {
			sendJsonError(400, "invalid id");
			return;
		}
		if (method == HTTP_PUT) {
			String newName = server.arg("newName");
			if (newName.length() == 0 && server.hasArg("plain")) {
				if (server.arg("plain").length() > MAX_JSON_BODY_SIZE) {
					sendJsonError(413, "payload too large");
					return;
				}
				cJSON *root = cJSON_Parse(server.arg("plain").c_str());
				if (root) {
					cJSON *item = cJSON_GetObjectItem(root, "newName");
					if (cJSON_IsString(item) && item->valuestring) {
						newName = item->valuestring;
					}
					cJSON_Delete(root);
				}
			}
			if (!isValidPlaylistName(newName)) {
				sendJsonError(400, "invalid newName");
				return;
			}
			StorageResult res = renamePlaylist(id, newName);
			if (res == StorageResult::Ok) {
				sendJsonOk();
			} else {
				sendStorageResult(res);
			}
			return;
		}
		if (method == HTTP_DELETE) {
			StorageResult res = deletePlaylist(id);
			if (res == StorageResult::Ok) {
				sendJsonOk();
			} else {
				sendStorageResult(res);
			}
			return;
		}
	}
	sendJsonError(404, "not found");
}

inline void initServer() {
	server.on("/", HTTP_GET, handleRoot);
	server.on("/now", HTTP_GET, handleNow);
	server.on("/api/mp3", HTTP_GET, handleApiMp3);
	server.on("/api/track/p", HTTP_GET, handleApiTrackPlaylists);
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

	server.on("/api/p", HTTP_GET, handleApiPlaylistsGet);
	server.on("/api/p", HTTP_POST, handleApiPlaylistsPost);
	server.on("/api/p/tracks", HTTP_PUT, handleApiPlaylistTracksUpdate);
	server.on("/api/p/tracks", HTTP_POST, handleApiPlaylistTracksAppend);

	server.onNotFound(handleDynamicPlaylists);
}

inline void handleServer() { server.handleClient(); }
