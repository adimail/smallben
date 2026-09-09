#pragma once
#include "Config.h"
#include "Dashboard.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>

WebServer server(HTTP_PORT);

static File uploadFile;
static bool uploadAborted = false;
static size_t uploadBytesWritten = 0;
const size_t MAX_UPLOAD_SIZE = 31457280;

inline bool isJunk(const String &name) {
	return name.startsWith(".") || name.startsWith("._") ||
		   name.equalsIgnoreCase("System Volume Information");
}

inline bool isMp3(String name) {
	name.toLowerCase();
	return name.endsWith(".mp3");
}

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

inline bool isSafePath(const String &path) {
	if (path.length() == 0 || path.length() > 128)
		return false;
	if (!path.startsWith("/"))
		return false;
	if (path.indexOf("..") != -1)
		return false;
	return true;
}

inline bool deleteFiles(const String &path) {
	if (!SD.exists(path))
		return false;

	File target = SD.open(path);
	if (!target)
		return false;

	if (!target.isDirectory()) {
		target.close();
		return SD.remove(path);
	}

	File entry = target.openNextFile();
	while (entry) {
		String childPath = entry.name();
		if (!childPath.startsWith("/")) {
			childPath = (path == "/" ? "" : path) + "/" + childPath;
		}
		bool isDir = entry.isDirectory();
		entry.close();

		if (isDir) {
			deleteFiles(childPath);
		} else {
			SD.remove(childPath);
		}
		entry = target.openNextFile();
	}
	target.close();
	return true;
}

inline void scanMp3Stream(File dir, const String &parentPath, bool &first) {
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;

		String name = entry.name();
		if (isJunk(name)) {
			entry.close();
			continue;
		}

		String fullPath = (parentPath == "/" ? "" : parentPath) + "/" + name;
		if (entry.isDirectory()) {
			scanMp3Stream(entry, fullPath, first);
		} else if (isMp3(name)) {
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("{\"name\":\"" + jsonEscape(name) + "\",\"path\":\"" +
							   jsonEscape(fullPath) + "\",\"size\":" + String(entry.size()) + "}");
		}
		entry.close();
	}
}

inline void scanTreeLevel(File dir) {
	server.sendContent("[");
	bool first = true;
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;

		String name = entry.name();
		if (isJunk(name)) {
			entry.close();
			continue;
		}

		if (!first)
			server.sendContent(",");
		first = false;

		if (entry.isDirectory()) {
			server.sendContent("{\"name\":\"" + jsonEscape(name) + "\",\"type\":\"dir\"}");
		} else {
			server.sendContent("{\"name\":\"" + jsonEscape(name) +
							   "\",\"type\":\"file\",\"size\":" + String(entry.size()) + "}");
		}
		entry.close();
	}
	server.sendContent("]");
}

inline void handleRoot() { server.send_P(200, "text/html", DASHBOARD_HTML); }

inline void handleApiMp3() {
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	File root = SD.open("/");
	bool first = true;
	server.sendContent("[");
	if (root) {
		scanMp3Stream(root, "/", first);
		root.close();
	}
	server.sendContent("]");
	server.sendContent("");
}

inline void handleApiTree() {
	String path = "/";
	if (server.hasArg("path")) {
		path = server.arg("path");
	}

	if (!isSafePath(path)) {
		server.send(400, "application/json", "{\"error\":\"invalid path\"}");
		return;
	}

	File dir = SD.open(path);
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		server.send(404, "application/json", "[]");
		return;
	}

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	scanTreeLevel(dir);
	dir.close();
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
	if (deleteFiles(path)) {
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
		server.send(400, "text/plain", "Invalid path");
		return;
	}
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
	if (lastSlash != -1) {
		filename = path.substring(lastSlash + 1);
	}

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
				uploadFile.close();
			} else {
				uploadFile.write(upload.buf, upload.currentSize);
			}
		}
	} else if (upload.status == UPLOAD_FILE_END) {
		if (uploadFile) {
			uploadFile.close();
		}
	} else if (upload.status == UPLOAD_FILE_ABORTED) {
		if (uploadFile) {
			uploadFile.close();
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

inline void initServer() {
	pinMode(SD_CS_PIN, OUTPUT);
	digitalWrite(SD_CS_PIN, HIGH);

	if (!SD.begin(SD_CS_PIN)) {
		Serial.println("[SD] Card initialization failed.");
	} else {
		Serial.println("[SD] Card initialized successfully.");
	}

	server.on("/", HTTP_GET, handleRoot);
	server.on("/api/mp3", HTTP_GET, handleApiMp3);
	server.on("/api/tree", HTTP_GET, handleApiTree);
	server.on("/api/delete", HTTP_DELETE, handleApiDelete);
	server.on("/api/download", HTTP_GET, handleApiDownload);
	server.on("/api/upload", HTTP_POST, handleFileUploadResponse, handleFileUploadProcess);

	server.begin();
	Serial.println("[HTTP] Server started.");
}

inline void handleServer() { server.handleClient(); }
