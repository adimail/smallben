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

static UploadSession uploadSession;
static bool uploadAborted = false;
static String uploadError = "";
static size_t uploadBytesWritten = 0;
static const size_t UPLOAD_CHUNK_BUF_SIZE = 4096;
static uint8_t *uploadChunkBuffer = nullptr;
static size_t uploadChunkBufferPos = 0;
static bool uploadAudioWasPlaying = false;
static bool uploadTrackOverwritten = false;

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
	std::vector<DiscMappingRecord> records;
	getAllDiscMappings(records);

	String json = "[";
	for (size_t i = 0; i < records.size(); i++) {
		if (i > 0)
			json += ",";
		json += "{\"uid\":\"" + jsonEscape(records[i].uid) + "\",\"type\":\"" +
				jsonEscape(records[i].type) + "\",\"target\":\"" + jsonEscape(records[i].target) +
				"\",\"label\":\"" + jsonEscape(records[i].label) + "\"}";
	}
	json += "]";
	sendJson(200, json);
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

inline void handleApiMp3() {
	heapLog("handleApiMp3:enter");
	int offset = 0;
	int limit = 50;
	if (server.hasArg("offset"))
		offset = server.arg("offset").toInt();
	if (server.hasArg("limit"))
		limit = server.arg("limit").toInt();
	if (offset < 0)
		offset = 0;
	if (limit <= 0 || limit > 100)
		limit = 50;

	std::vector<StorageEntry> entries;
	int totalCount = 0;
	bool hasMore = false;

	StorageResult res = getMp3TracksPaginated(offset, limit, entries, totalCount, hasMore);
	if (res != StorageResult::Ok) {
		sendJson(503, "{\"total\":0,\"entries\":[]}");
		return;
	}

	String json = "{\"offset\":" + String(offset) + ",\"limit\":" + String(limit) +
				  ",\"total\":" + String(totalCount) +
				  ",\"hasMore\":" + (hasMore ? "true" : "false") + ",\"entries\":[";
	for (size_t i = 0; i < entries.size(); i++) {
		if (i > 0)
			json += ",";
		json += "{\"name\":\"" + jsonEscape(entries[i].name) + "\",\"path\":\"" +
				jsonEscape(entries[i].path) + "\",\"size\":" + String(entries[i].size) + "}";
	}
	json += "]}";
	sendJson(200, json);
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

	std::vector<String> matched = getPlaylistsContainingTrack(path);
	String json = "[";
	for (size_t i = 0; i < matched.size(); i++) {
		if (i > 0)
			json += ",";
		json += "\"" + jsonEscape(matched[i]) + "\"";
	}
	json += "]";
	sendJson(200, json);
}

inline void handleApiTree() {
	String path = "/";
	if (server.hasArg("path"))
		path = normalizePath(server.arg("path"));
	if (!isValidPath(path)) {
		sendJsonError(400, "invalid path");
		return;
	}

	int offset = 0;
	int limit = 30;
	if (server.hasArg("offset"))
		offset = server.arg("offset").toInt();
	if (server.hasArg("limit"))
		limit = server.arg("limit").toInt();
	if (offset < 0)
		offset = 0;
	if (limit <= 0 || limit > 100)
		limit = 30;

	std::vector<StorageEntry> entries;
	int totalCount = 0;
	bool hasMore = false;

	StorageResult res = getDirectoryEntries(path, offset, limit, entries, totalCount, hasMore);
	if (res != StorageResult::Ok) {
		sendJson(404, "{\"total\":0,\"entries\":[]}");
		return;
	}

	String json = "{\"path\":\"" + jsonEscape(path) + "\",\"offset\":" + String(offset) +
				  ",\"limit\":" + String(limit) + ",\"total\":" + String(totalCount) +
				  ",\"hasMore\":" + (hasMore ? "true" : "false") + ",\"entries\":[";
	for (size_t i = 0; i < entries.size(); i++) {
		if (i > 0)
			json += ",";
		if (entries[i].isDir) {
			json += "{\"name\":\"" + jsonEscape(entries[i].name) + "\",\"type\":\"dir\"}";
		} else {
			json += "{\"name\":\"" + jsonEscape(entries[i].name) +
					"\",\"type\":\"file\",\"size\":" + String(entries[i].size) + "}";
		}
	}
	json += "]}";
	sendJson(200, json);
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

	String filename = path;
	int lastSlash = path.lastIndexOf('/');
	if (lastSlash != -1)
		filename = path.substring(lastSlash + 1);

	WiFiClient client = server.client();
	size_t fileSize = 0;
	server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");

	StorageResult res = streamFileToClient(path, client, fileSize);
	if (res != StorageResult::Ok) {
		triggerErrorAudio();
		server.send(404, "text/plain", "File not found");
	}
}

inline void flushUploadChunkBuffer() {
	if (uploadChunkBufferPos > 0) {
		StorageResult res =
			writeUploadSession(uploadSession, uploadChunkBuffer, uploadChunkBufferPos);
		if (res != StorageResult::Ok) {
			uploadAborted = true;
			uploadError = "WRITE_FAILED";
		}
		uploadChunkBufferPos = 0;
	}
}

inline void handleFileUploadProcess() {
	HTTPUpload &upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		heapLog("upload:START");
		uploadAborted = false;
		uploadError = "";
		uploadBytesWritten = 0;
		uploadChunkBufferPos = 0;

		AudioState aState = getAudioState();
		uploadAudioWasPlaying = (aState.isPlaying && !aState.isPaused);
		uploadTrackOverwritten = false;
		if (uploadAudioWasPlaying) {
			requestAudioPause();
		}
		setRfidEnabled(false);

		if (!uploadChunkBuffer) {
			uploadChunkBuffer = (uint8_t *)malloc(UPLOAD_CHUNK_BUF_SIZE);
			if (!uploadChunkBuffer) {
				uploadAborted = true;
				uploadError = "OUT_OF_MEMORY";
				return;
			}
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

		String finalPath = (targetDir == "/" ? "" : targetDir) + "/" + filename;
		if (isProtectedPath(finalPath)) {
			uploadAborted = true;
			uploadError = "PROTECTED_PATH";
			return;
		}

		if (aState.isPlaying && (finalPath == String(aState.path))) {
			uploadTrackOverwritten = true;
		}

		StorageResult res = openUploadSession(finalPath, uploadSession);
		if (res != StorageResult::Ok) {
			uploadAborted = true;
			uploadError = "TEMP_OPEN_FAILED";
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (!uploadAborted) {
			size_t inLen = upload.currentSize;
			size_t inOff = 0;

			while (inLen > 0 && !uploadAborted) {
				size_t space = UPLOAD_CHUNK_BUF_SIZE - uploadChunkBufferPos;
				size_t take = (inLen < space) ? inLen : space;

				memcpy(uploadChunkBuffer + uploadChunkBufferPos, upload.buf + inOff, take);
				uploadChunkBufferPos += take;
				inOff += take;
				inLen -= take;

				if (uploadChunkBufferPos >= UPLOAD_CHUNK_BUF_SIZE) {
					flushUploadChunkBuffer();
				}
			}

			uploadBytesWritten += upload.currentSize;
			if (uploadBytesWritten > MAX_UPLOAD_SIZE) {
				uploadAborted = true;
				uploadError = "FILE_TOO_LARGE";
				closeUploadSession(uploadSession, false);
			}
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	} else if (upload.status == UPLOAD_FILE_END) {
		heapLog("upload:END before commit");
		flushUploadChunkBuffer();
		if (uploadChunkBuffer) {
			free(uploadChunkBuffer);
			uploadChunkBuffer = nullptr;
		}
		if (!uploadAborted) {
			StorageResult res = closeUploadSession(uploadSession, true);
			heapLog("upload:END after commit");
			if (res != StorageResult::Ok) {
				uploadAborted = true;
				uploadError = "COMMIT_FAILED";
			}
		} else {
			closeUploadSession(uploadSession, false);
		}

		if (uploadAudioWasPlaying) {
			if (uploadTrackOverwritten) {
				requestAudioStop();
			} else {
				requestAudioResume();
			}
		}
		uploadAudioWasPlaying = false;
		uploadTrackOverwritten = false;
		setRfidEnabled(true);
	} else if (upload.status == UPLOAD_FILE_ABORTED) {
		if (uploadChunkBuffer) {
			free(uploadChunkBuffer);
			uploadChunkBuffer = nullptr;
		}
		closeUploadSession(uploadSession, false);
		uploadAborted = true;
		if (uploadError.length() == 0) {
			uploadError = "ABORTED";
		}
		if (uploadAudioWasPlaying) {
			requestAudioResume();
		}
		uploadAudioWasPlaying = false;
		uploadTrackOverwritten = false;
		setRfidEnabled(true);
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
	std::vector<PlaylistSummary> list;
	StorageResult res = getAllPlaylists(list);
	if (res != StorageResult::Ok) {
		sendJson(503, "[]");
		return;
	}

	String json = "[";
	for (size_t i = 0; i < list.size(); i++) {
		if (i > 0)
			json += ",";
		json += "{\"name\":\"" + jsonEscape(list[i].name) +
				"\",\"trackCount\":" + String(list[i].trackCount) + "}";
	}
	json += "]";
	sendJson(200, json);
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

	std::vector<PlaylistTrackInfo> tracks;
	StorageResult res = getPlaylistTracks(id, tracks);
	if (res != StorageResult::Ok) {
		sendJsonError(404, "not found");
		return;
	}

	String json = "[";
	for (size_t i = 0; i < tracks.size(); i++) {
		if (i > 0)
			json += ",";
		json += "{\"name\":\"" + jsonEscape(tracks[i].name) + "\",\"path\":\"" +
				jsonEscape(tracks[i].path) + "\",\"size\":" + String(tracks[i].size) +
				",\"exists\":" + (tracks[i].exists ? "true" : "false") + "}";
	}
	json += "]";
	sendJson(200, json);
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
	cJSON *tracksArr = cJSON_GetObjectItem(root, "tracks");
	if (!cJSON_IsArray(tracksArr)) {
		cJSON_Delete(root);
		sendJsonError(400, "missing tracks");
		return;
	}

	std::vector<String> tracksList;
	cJSON *elem = nullptr;
	cJSON_ArrayForEach(elem, tracksArr) {
		if (cJSON_IsString(elem) && elem->valuestring) {
			tracksList.push_back(String(elem->valuestring));
		}
	}
	cJSON_Delete(root);

	StorageResult res = setPlaylistTracks(id, tracksList);
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
