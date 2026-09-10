#pragma once
#include "AudioEngine.h"
#include "Config.h"
#include "Dashboard.h"
#include "RfidEngine.h"
#include "Storage.h"
#include <Arduino.h>
#include <WebServer.h>
#include <vector>

WebServer server(HTTP_PORT);

static File uploadFile;
static bool uploadAborted = false;
static size_t uploadBytesWritten = 0;

inline String jsonEscape(const String &in) {
	String out;
	out.reserve(in.length() + 4);
	for (size_t i = 0; i < in.length(); i++) {
		char c = in[i];
		if (c == '"' || c == '\\')
			out += '\\';
		out += c;
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
	int q2 = json.indexOf('"', q1 + 1);
	if (q2 == -1)
		return "";
	return json.substring(q1 + 1, q2);
}

inline std::vector<String> extractJsonArray(const String &json, const String &key) {
	std::vector<String> res;
	String needle = "\"" + key + "\"";
	int kpos = json.indexOf(needle);
	if (kpos == -1)
		return res;
	int b1 = json.indexOf('[', kpos + needle.length());
	int b2 = json.indexOf(']', b1 + 1);
	if (b1 == -1 || b2 == -1)
		return res;
	String arr = json.substring(b1 + 1, b2);
	int cur = 0;
	while (cur < (int)arr.length()) {
		int q1 = arr.indexOf('"', cur);
		if (q1 == -1)
			break;
		int q2 = arr.indexOf('"', q1 + 1);
		if (q2 == -1)
			break;
		res.push_back(arr.substring(q1 + 1, q2));
		cur = q2 + 1;
	}
	return res;
}

inline void handleRoot() { server.send_P(200, "text/html", DASHBOARD_HTML); }

inline void handleNow() {
	String path = getAudioPath();
	String name = "";
	int slash = path.lastIndexOf('/');
	if (slash != -1) {
		name = path.substring(slash + 1);
	} else {
		name = path;
	}

	String json = "{";
	json += "\"player\":{";
	json += "\"state\":\"" + getAudioState() + "\",";
	json += "\"path\":\"" + jsonEscape(path) + "\",";
	json += "\"name\":\"" + jsonEscape(name) + "\"";
	json += "},";
	json += "\"sd\":{";
	json += "\"ready\":" + String(isSdReady() ? "true" : "false") + ",";
	json += "\"totalBytes\":" + String((unsigned long long)getSdTotalBytes()) + ",";
	json += "\"usedBytes\":" + String((unsigned long long)getSdUsedBytes());
	json += "},";
	json += "\"rfid\":{";
	json += "\"present\":" + String(isRfidPresent() ? "true" : "false") + ",";
	json += "\"uid\":\"" + jsonEscape(getRfidUid()) + "\",";
	json += "\"mapped\":" + String(isRfidMapped() ? "true" : "false") + ",";
	json += "\"type\":\"" + jsonEscape(getRfidType()) + "\",";
	json += "\"target\":\"" + jsonEscape(getRfidTarget()) + "\",";
	json += "\"label\":\"" + jsonEscape(getRfidLabel()) + "\"";
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
	for (const auto &pair : g_discMappings) {
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"uid\":\"" + jsonEscape(pair.second.uid) + "\",\"type\":\"" +
						   jsonEscape(pair.second.type) + "\",\"target\":\"" +
						   jsonEscape(pair.second.target) + "\",\"label\":\"" +
						   jsonEscape(pair.second.label) + "\"}");
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiDiscsMap() {
	String body = server.arg("plain");
	String uid = extractJsonString(body, "uid");
	String type = extractJsonString(body, "type");
	String target = extractJsonString(body, "target");
	String label = extractJsonString(body, "label");
	if (mapDisc(uid, type, target, label)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(400, "application/json", "{\"error\":\"invalid parameters\"}");
	}
}

inline void handleApiDiscsDelete() {
	String uid = "";
	if (server.hasArg("uid")) {
		uid = server.arg("uid");
	} else if (server.hasArg("plain")) {
		uid = extractJsonString(server.arg("plain"), "uid");
	}
	if (unmapDisc(uid)) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(404, "application/json", "{\"error\":\"not found\"}");
	}
}

inline void handleApiMp3() {
	rebuildTrackCacheIfNeeded();
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	for (const auto &t : g_trackCache) {
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"name\":\"" + jsonEscape(t.name) + "\",\"path\":\"" +
						   jsonEscape(t.path) + "\",\"size\":" + String(t.size) + "}");
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
	rebuildPlaylistCacheIfNeeded();
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	auto it = g_trackPlaylistIndex.find(path);
	if (it != g_trackPlaylistIndex.end()) {
		for (const String &name : it->second) {
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("\"" + jsonEscape(name) + "\"");
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
	SdGuard guard;
	File dir = SD.open(path);
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

inline void handleApiDelete() {
	if (!server.hasArg("path")) {
		server.send(400, "application/json", "{\"error\":\"missing path\"}");
		return;
	}
	String path = server.arg("path");
	if (!isSafePath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	if (deleteStoragePath(path)) {
		invalidateTrackCache();
		invalidatePlaylistCache();
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(500, "application/json", "{\"error\":\"delete failed\"}");
	}
}

inline void handleApiDownload() {
	if (!server.hasArg("path")) {
		server.send(400, "text/plain", "Missing path parameter");
		return;
	}
	String path = server.arg("path");
	if (!isSafePath(path)) {
		server.send(404, "text/plain", "File not found");
		return;
	}
	SdGuard guard;
	if (!SD.exists(path)) {
		server.send(404, "text/plain", "File not found");
		return;
	}
	File file = SD.open(path, FILE_READ);
	if (!file || file.isDirectory()) {
		if (file)
			file.close();
		server.send(400, "text/plain", "Cannot download directory");
		return;
	}
	String filename = path;
	int lastSlash = path.lastIndexOf('/');
	if (lastSlash != -1)
		filename = path.substring(lastSlash + 1);
	server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
	server.streamFile(file, "application/octet-stream");
	file.close();
}

inline void handleFileUploadProcess() {
	HTTPUpload &upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		uploadAborted = false;
		uploadBytesWritten = 0;
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

		String fullPath = (targetDir == "/" ? "" : targetDir) + "/" + filename;
		if (isProtectedStoragePath(fullPath)) {
			uploadAborted = true;
			return;
		}
		SdGuard guard;
		if (SD.exists(fullPath)) {
			SD.remove(fullPath);
		}
		uploadFile = SD.open(fullPath, FILE_WRITE);
		if (!uploadFile) {
			uploadAborted = true;
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (!uploadAborted && uploadFile) {
			uploadBytesWritten += upload.currentSize;
			if (uploadBytesWritten > MAX_UPLOAD_SIZE) {
				uploadAborted = true;
				SdGuard guard;
				uploadFile.close();
			} else {
				SdGuard guard;
				uploadFile.write(upload.buf, upload.currentSize);
			}
		}
	} else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
		if (uploadFile) {
			SdGuard guard;
			uploadFile.close();
		}
		if (upload.status == UPLOAD_FILE_END && !uploadAborted) {
			invalidateTrackCache();
		}
	}
}

inline void handleFileUploadResponse() {
	if (uploadAborted) {
		server.send(413, "application/json", "{\"error\":\"upload failed or exceeded limit\"}");
	} else {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	}
}

inline void handleApiPlaylistsGet() {
	rebuildPlaylistCacheIfNeeded();
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	server.sendContent("[");
	bool first = true;
	for (const auto &p : g_playlistCache) {
		if (!first)
			server.sendContent(",");
		first = false;
		server.sendContent("{\"name\":\"" + jsonEscape(p.name) +
						   "\",\"trackCount\":" + String(p.trackCount) + "}");
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiPlaylistsPost() {
	String body = server.arg("plain");
	String name = extractJsonString(body, "name");
	if (!isSafePlaylistName(name)) {
		server.send(400, "application/json", "{\"error\":\"invalid name\"}");
		return;
	}
	if (createPlaylist(name)) {
		invalidatePlaylistCache();
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
	String id = urldecode(server.arg("id"));
	if (!isSafePlaylistName(id)) {
		server.send(400, "application/json", "{\"error\":\"invalid id\"}");
		return;
	}
	String body = server.arg("plain");
	std::vector<String> tracks = extractJsonArray(body, "tracks");
	if (overwritePlaylistTracks(id, tracks)) {
		invalidatePlaylistCache();
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(500, "application/json", "{\"error\":\"write failed\"}");
	}
}

inline void handleApiPlaylistTracksAppend() {
	if (!server.hasArg("id")) {
		server.send(400, "application/json", "{\"error\":\"missing id\"}");
		return;
	}
	String id = urldecode(server.arg("id"));
	if (!isSafePlaylistName(id)) {
		server.send(400, "application/json", "{\"error\":\"invalid id\"}");
		return;
	}
	String body = server.arg("plain");
	String path = extractJsonString(body, "path");
	if (!isSafePath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	if (appendPlaylistTrack(id, path)) {
		invalidatePlaylistCache();
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	} else {
		server.send(500, "application/json", "{\"error\":\"append failed\"}");
	}
}

inline void handleApiPlayerToggle() {
	String path = "";
	if (server.hasArg("path")) {
		path = server.arg("path");
	} else if (server.hasArg("plain")) {
		path = extractJsonString(server.arg("plain"), "path");
	}
	if (path.length() == 0 || !isSafePath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}
	if (togglePlayAudio(path)) {
		server.send(200, "application/json",
					"{\"status\":\"ok\",\"state\":\"" + getAudioState() + "\",\"path\":\"" +
						jsonEscape(getAudioPath()) + "\"}");
	} else {
		server.send(500, "application/json", "{\"error\":\"playback failed\"}");
	}
}

inline void handleApiPlayerStatus() {
	server.send(200, "application/json",
				"{\"status\":\"ok\",\"state\":\"" + getAudioState() + "\",\"path\":\"" +
					jsonEscape(getAudioPath()) + "\"}");
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
			String body = server.arg("plain");
			String newName = extractJsonString(body, "newName");
			if (!isSafePlaylistName(newName)) {
				server.send(400, "application/json", "{\"error\":\"invalid newName\"}");
				return;
			}
			if (renamePlaylist(id, newName)) {
				invalidatePlaylistCache();
				server.send(200, "application/json", "{\"status\":\"ok\"}");
			} else {
				server.send(500, "application/json", "{\"error\":\"rename failed\"}");
			}
			return;
		}
		if (method == HTTP_DELETE) {
			if (deletePlaylist(id)) {
				invalidatePlaylistCache();
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
	initStorage();

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

	server.begin();
	Serial.println("[HTTP] Server started.");
}

inline void handleServer() { server.handleClient(); }
