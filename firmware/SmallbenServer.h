#pragma once
#include "Config.h"
#include "Dashboard.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>

// WebServer instance
WebServer server(HTTP_PORT);

// ---------- SD Helpers ----------

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

// Only allow absolute paths with no ".." traversal, and keep them short.
// This is called before every /api/tree lookup so a client can never make
// the ESP32 walk outside the SD card or loop on a crafted path.
inline bool isSafePath(const String &path) {
	if (path.length() == 0 || path.length() > 128)
		return false;
	if (!path.startsWith("/"))
		return false;
	if (path.indexOf("..") != -1)
		return false;
	return true;
}

// Recursively streams flat list of MP3 files
inline void scanMp3Stream(File dir, bool &first) {
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;

		String name = entry.name();
		if (isJunk(name)) {
			entry.close();
			continue;
		}

		if (entry.isDirectory()) {
			scanMp3Stream(entry, first);
		} else if (isMp3(name)) {
			if (!first)
				server.sendContent(",");
			first = false;
			server.sendContent("{\"name\":\"" + jsonEscape(name) +
							   "\",\"size\":" + String(entry.size()) + "}");
		}
		entry.close();
	}
}

// Streams ONE level of a directory only (no recursion). Subfolders are
// reported as {"type":"dir"} with no children — the browser fetches a
// subfolder's contents in a separate request only when the user opens it.
// This keeps each response small and bounded no matter how big or deep
// the SD card's contents are.
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

// ---------- API Endpoint Handlers ----------

inline void handleRoot() { server.send_P(200, "text/html", DASHBOARD_HTML); }

inline void handleApiMp3() {
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "application/json", "");
	File root = SD.open("/");
	bool first = true;
	server.sendContent("[");
	if (root) {
		scanMp3Stream(root, first);
		root.close();
	}
	server.sendContent("]");
	server.sendContent(""); // Terminate transfer
}

inline void handleApiTree() {
	// Which folder to list — defaults to root. The browser passes this
	// only when the user actually opens a folder, so nothing is fetched
	// until it's needed.
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
	server.sendContent(""); // Terminate transfer
}

// ---------- Public Lifecycle Functions ----------

inline void initServer() {
	// Initialize SD Card
	if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
		Serial.println("[SD] Card initialization failed.");
	} else {
		Serial.println("[SD] Card initialized.");
	}

	// Register Web Routes
	server.on("/", HTTP_GET, handleRoot);
	server.on("/api/mp3", HTTP_GET, handleApiMp3);
	server.on("/api/tree", HTTP_GET, handleApiTree);

	server.begin();
	Serial.println("[HTTP] Server started.");
}

inline void handleServer() { server.handleClient(); }
