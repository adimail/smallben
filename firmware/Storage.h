#pragma once
#include "Config.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <map>
#include <vector>

static SemaphoreHandle_t sdMutex = nullptr;

class SdGuard {
  public:
	SdGuard() { xSemaphoreTakeRecursive(sdMutex, portMAX_DELAY); }
	~SdGuard() { xSemaphoreGiveRecursive(sdMutex); }
	SdGuard(const SdGuard &) = delete;
	SdGuard &operator=(const SdGuard &) = delete;
};

struct TrackInfo {
	String name;
	String path;
	size_t size;
};

struct PlaylistInfo {
	String name;
	int trackCount;
};

static std::vector<TrackInfo> g_trackCache;
static bool g_trackCacheValid = false;

static std::vector<PlaylistInfo> g_playlistCache;
static bool g_playlistCacheValid = false;

static std::map<String, std::vector<String>> g_trackPlaylistIndex;

inline void invalidateTrackCache() { g_trackCacheValid = false; }

inline void invalidatePlaylistCache() { g_playlistCacheValid = false; }

inline String urldecode(const String &src) {
	String decoded = "";
	char a, b;
	for (size_t i = 0; i < src.length(); i++) {
		if ((src[i] == '%') && (i + 2 < src.length()) && isxdigit(src[i + 1]) &&
			isxdigit(src[i + 2])) {
			a = src[i + 1];
			b = src[i + 2];
			if (a >= 'a')
				a -= 'a' - 'A';
			if (a >= 'A')
				a -= ('A' - 10);
			else
				a -= '0';
			if (b >= 'a')
				b -= 'a' - 'A';
			if (b >= 'A')
				b -= ('A' - 10);
			else
				b -= '0';
			decoded += (char)(16 * a + b);
			i += 2;
		} else if (src[i] == '+') {
			decoded += ' ';
		} else {
			decoded += src[i];
		}
	}
	return decoded;
}

inline bool initStorage() {
	if (!sdMutex) {
		sdMutex = xSemaphoreCreateRecursiveMutex();
	}
	pinMode(SD_CS_PIN, OUTPUT);
	digitalWrite(SD_CS_PIN, HIGH);
	if (!SD.begin(SD_CS_PIN)) {
		return false;
	}
	if (!SD.exists("/playlists")) {
		SD.mkdir("/playlists");
	}
	return true;
}

inline bool isJunk(String name) {
	for (size_t i = 0; i < NUM_HIDDEN_PREFIXES; i++) {
		if (name.startsWith(HIDDEN_PREFIXES[i])) {
			return true;
		}
	}

	String lower = name;
	lower.toLowerCase();

	for (size_t i = 0; i < NUM_HIDDEN_EXACT_NAMES; i++) {
		if (lower == HIDDEN_EXACT_NAMES[i]) {
			return true;
		}
	}

	for (size_t i = 0; i < NUM_HIDDEN_EXTENSIONS; i++) {
		if (lower.endsWith(HIDDEN_EXTENSIONS[i])) {
			return true;
		}
	}

	return false;
}

inline bool isProtectedStoragePath(const String &path) {
	String lower = path;
	lower.toLowerCase();

	while (lower.startsWith("/")) {
		lower = lower.substring(1);
	}
	while (lower.endsWith("/")) {
		lower = lower.substring(0, lower.length() - 1);
	}

	for (size_t i = 0; i < NUM_HIDDEN_EXACT_NAMES; i++) {
		String target = HIDDEN_EXACT_NAMES[i];
		if (lower == target || lower.startsWith(target + "/")) {
			return true;
		}
	}

	for (size_t i = 0; i < NUM_HIDDEN_EXTENSIONS; i++) {
		if (lower.endsWith(HIDDEN_EXTENSIONS[i])) {
			return true;
		}
	}

	int lastSlash = path.lastIndexOf('/');
	String filename = (lastSlash != -1) ? path.substring(lastSlash + 1) : path;
	for (size_t i = 0; i < NUM_HIDDEN_PREFIXES; i++) {
		if (filename.startsWith(HIDDEN_PREFIXES[i])) {
			return true;
		}
	}

	return false;
}

inline bool isMp3(String name) {
	name.toLowerCase();
	return name.endsWith(".mp3");
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

inline bool isSafePlaylistName(const String &name) {
	if (name.length() == 0 || name.length() > 64)
		return false;
	if (name.indexOf('/') != -1 || name.indexOf('\\') != -1 || name.indexOf("..") != -1)
		return false;
	return true;
}

inline String getPlaylistPath(String name) {
	if (!name.endsWith(".m3u"))
		name += ".m3u";
	return "/playlists/" + name;
}

inline bool deleteStoragePath(const String &path) {
	SdGuard guard;
	if (isProtectedStoragePath(path)) {
		return false;
	}
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
			deleteStoragePath(childPath);
		} else {
			if (!isProtectedStoragePath(childPath)) {
				SD.remove(childPath);
			}
		}
		entry = target.openNextFile();
	}
	target.close();
	return SD.rmdir(path);
}

template <typename Callback>
inline void iterateMp3Tracks(File dir, const String &parentPath, Callback cb) {
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
			iterateMp3Tracks(entry, fullPath, cb);
		} else if (isMp3(name)) {
			cb(name, fullPath, entry.size());
		}
		entry.close();
	}
}

template <typename Callback> inline void iterateDirectory(File dir, Callback cb) {
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;
		String name = entry.name();
		if (isJunk(name)) {
			entry.close();
			continue;
		}
		cb(name, entry.isDirectory(), entry.size());
		entry.close();
	}
}

inline void rebuildTrackCacheIfNeeded() {
	if (g_trackCacheValid)
		return;
	std::vector<TrackInfo> fresh;
	{
		SdGuard guard;
		File root = SD.open("/");
		if (root) {
			iterateMp3Tracks(root, "/", [&](const String &name, const String &path, size_t sz) {
				fresh.push_back({name, path, sz});
			});
			root.close();
		}
	}
	g_trackCache = fresh;
	g_trackCacheValid = true;
}

inline bool getCachedTrackSize(const String &path, size_t &sizeOut) {
	rebuildTrackCacheIfNeeded();
	for (const auto &t : g_trackCache) {
		if (t.path == path) {
			sizeOut = t.size;
			return true;
		}
	}
	return false;
}

inline void rebuildPlaylistCacheIfNeeded() {
	if (g_playlistCacheValid)
		return;
	std::vector<PlaylistInfo> freshPlaylists;
	std::map<String, std::vector<String>> freshIndex;
	{
		SdGuard guard;
		File dir = SD.open("/playlists");
		if (dir) {
			while (true) {
				File entry = dir.openNextFile();
				if (!entry)
					break;
				String name = entry.name();
				String lower = name;
				lower.toLowerCase();
				if (!entry.isDirectory() && lower.endsWith(".m3u") && !name.startsWith(".") &&
					!name.startsWith("._")) {
					String fullPath = "/playlists/" + name;
					String base = name;
					base.replace(".m3u", "");
					int count = 0;
					File f = SD.open(fullPath, FILE_READ);
					if (f) {
						while (f.available()) {
							String line = f.readStringUntil('\n');
							line.trim();
							if (line.length() > 0 && !line.startsWith("#")) {
								count++;
								freshIndex[line].push_back(base);
							}
						}
						f.close();
					}
					freshPlaylists.push_back({name, count});
				}
				entry.close();
			}
			dir.close();
		}
	}
	g_playlistCache = freshPlaylists;
	g_trackPlaylistIndex = freshIndex;
	g_playlistCacheValid = true;
}

inline int countPlaylistTracks(const String &path) {
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return 0;
	int count = 0;
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() > 0 && !line.startsWith("#")) {
			count++;
		}
	}
	f.close();
	return count;
}

template <typename Callback> inline void iteratePlaylists(Callback cb) {
	SdGuard guard;
	File dir = SD.open("/playlists");
	if (!dir)
		return;
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;
		String name = entry.name();
		String lower = name;
		lower.toLowerCase();
		if (!entry.isDirectory() && lower.endsWith(".m3u") && !name.startsWith(".") &&
			!name.startsWith("._")) {
			String fullPath = "/playlists/" + name;
			int tracks = countPlaylistTracks(fullPath);
			cb(name, tracks);
		}
		entry.close();
	}
	dir.close();
}

template <typename Callback> inline bool iteratePlaylistTracks(const String &name, Callback cb) {
	String path = getPlaylistPath(name);
	std::vector<String> lines;
	{
		SdGuard guard;
		File f = SD.open(path, FILE_READ);
		if (!f)
			return false;
		while (f.available()) {
			String trackPath = f.readStringUntil('\n');
			trackPath.trim();
			if (trackPath.length() > 0 && !trackPath.startsWith("#")) {
				lines.push_back(trackPath);
			}
		}
		f.close();
	}
	for (const String &trackPath : lines) {
		int lastSlash = trackPath.lastIndexOf('/');
		String trackName = (lastSlash != -1) ? trackPath.substring(lastSlash + 1) : trackPath;
		size_t sz = 0;
		bool exists = getCachedTrackSize(trackPath, sz);
		cb(trackName, trackPath, sz, exists);
	}
	return true;
}

inline bool createPlaylist(const String &name) {
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (SD.exists(path))
		return false;
	File f = SD.open(path, FILE_WRITE);
	if (!f)
		return false;
	f.close();
	return true;
}

inline bool renamePlaylist(const String &oldName, const String &newName) {
	SdGuard guard;
	String oldPath = getPlaylistPath(oldName);
	String newPath = getPlaylistPath(newName);
	if (!SD.exists(oldPath) || SD.exists(newPath))
		return false;
	if (SD.rename(oldPath, newPath))
		return true;
	File src = SD.open(oldPath, FILE_READ);
	if (!src)
		return false;
	File dst = SD.open(newPath, FILE_WRITE);
	if (!dst) {
		src.close();
		return false;
	}
	uint8_t buf[512];
	while (src.available()) {
		size_t bytesRead = src.read(buf, sizeof(buf));
		dst.write(buf, bytesRead);
	}
	src.close();
	dst.close();
	SD.remove(oldPath);
	return true;
}

inline bool deletePlaylist(const String &name) {
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (!SD.exists(path))
		return false;
	return SD.remove(path);
}

inline bool appendPlaylistTrack(const String &name, const String &trackPath) {
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (!SD.exists(path) || !isSafePath(trackPath))
		return false;
	File f = SD.open(path, FILE_APPEND);
	if (!f)
		return false;
	f.println(trackPath);
	f.close();
	return true;
}

inline bool overwritePlaylistTracks(const String &name, const std::vector<String> &tracks) {
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (!SD.exists(path))
		return false;
	File f = SD.open(path, FILE_WRITE);
	if (!f)
		return false;
	for (const String &tp : tracks) {
		if (isSafePath(tp)) {
			f.println(tp);
		}
	}
	f.close();
	return true;
}
