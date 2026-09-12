#pragma once
#include "Config.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <map>
#include <vector>

enum class SdStorageState { Ready, Busy, Error };
static volatile SdStorageState g_sdStorageState = SdStorageState::Error;

static SPIClass sdSPI(HSPI);
static SemaphoreHandle_t sdMutex = nullptr;
static bool g_sdReady = false;

static uint64_t g_cachedTotalBytes = 0;
static uint64_t g_cachedUsedBytes = 0;
static unsigned long g_lastSdStatsUpdate = 0;

static char g_reservedAudioPath[128] = {0};
static bool (*g_isAudioPathActiveCheck)(const String &path) = nullptr;

inline void ensureSdMutex() {
	if (!sdMutex) {
		sdMutex = xSemaphoreCreateRecursiveMutex();
	}
}

class SdGuard {
  public:
	SdGuard() {
		ensureSdMutex();
		if (sdMutex) {
			xSemaphoreTakeRecursive(sdMutex, portMAX_DELAY);
		}
	}
	~SdGuard() {
		if (sdMutex) {
			xSemaphoreGiveRecursive(sdMutex);
		}
	}
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

inline SdStorageState getSdStorageState() { return g_sdStorageState; }
inline void setSdStorageState(SdStorageState state) { g_sdStorageState = state; }
inline bool isSdReady() { return g_sdStorageState == SdStorageState::Ready; }

inline void setAudioPathActiveChecker(bool (*fn)(const String &)) { g_isAudioPathActiveCheck = fn; }

inline void setAudioPathReserved(const char *path) {
	if (path) {
		strncpy(g_reservedAudioPath, path, sizeof(g_reservedAudioPath) - 1);
		g_reservedAudioPath[sizeof(g_reservedAudioPath) - 1] = '\0';
	} else {
		g_reservedAudioPath[0] = '\0';
	}
}

inline void clearAudioPathReserved() { g_reservedAudioPath[0] = '\0'; }

inline bool isAudioPathCurrentlyActive(const String &path) {
	if (g_reservedAudioPath[0] != '\0') {
		String reserved(g_reservedAudioPath);
		if (reserved == path)
			return true;
		if (path != "/" && reserved.startsWith(path + "/"))
			return true;
	}
	if (g_isAudioPathActiveCheck) {
		return g_isAudioPathActiveCheck(path);
	}
	return false;
}

inline String urldecode(const String &src) {
	String decoded = "";
	decoded.reserve(src.length());
	for (size_t i = 0; i < src.length(); i++) {
		if ((src[i] == '%') && (i + 2 < src.length()) && isxdigit(src[i + 1]) &&
			isxdigit(src[i + 2])) {
			char a = src[i + 1];
			char b = src[i + 2];
			if (a >= 'a' && a <= 'f')
				a -= ('a' - 10);
			else if (a >= 'A' && a <= 'F')
				a -= ('A' - 10);
			else
				a -= '0';

			if (b >= 'a' && b <= 'f')
				b -= ('a' - 10);
			else if (b >= 'A' && b <= 'F')
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

inline void refreshSdStatsDirect() {
	if (g_sdStorageState == SdStorageState::Error) {
		g_cachedTotalBytes = 0;
		g_cachedUsedBytes = 0;
		return;
	}
	SdGuard guard;
	g_cachedTotalBytes = SD.totalBytes();
	g_cachedUsedBytes = SD.usedBytes();
	g_lastSdStatsUpdate = millis();
}

inline void refreshSdStatsIfNeeded() {
	if (!isSdReady())
		return;
	if (millis() - g_lastSdStatsUpdate > 45000 || g_cachedTotalBytes == 0) {
		refreshSdStatsDirect();
	}
}

inline uint64_t getSdFreeBytes() {
	refreshSdStatsIfNeeded();
	if (g_cachedTotalBytes > g_cachedUsedBytes) {
		return g_cachedTotalBytes - g_cachedUsedBytes;
	}
	return 0;
}

inline void cleanupOrphanedTempFiles(File dir, const String &parentPath) {
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;
		String name = entry.name();
		String childPath = (parentPath == "/" ? "" : parentPath) + "/" + name;
		bool isDir = entry.isDirectory();
		entry.close();
		if (isDir) {
			File childDir = SD.open(childPath);
			if (childDir) {
				cleanupOrphanedTempFiles(childDir, childPath);
				childDir.close();
			}
		} else {
			String lower = name;
			lower.toLowerCase();
			if (lower.endsWith(".uploading") || lower.endsWith(".tmp") || lower.endsWith(".new")) {
				SD.remove(childPath);
			}
		}
	}
}

inline void performStorageBootCleanup() {
	SdGuard guard;
	File root = SD.open("/");
	if (root) {
		cleanupOrphanedTempFiles(root, "/");
		root.close();
	}
}

inline bool initStorage() {
	ensureSdMutex();
	pinMode(SD_CS_PIN, OUTPUT);
	digitalWrite(SD_CS_PIN, HIGH);
	sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
	g_sdReady = SD.begin(SD_CS_PIN, sdSPI);
	if (!g_sdReady) {
		g_sdStorageState = SdStorageState::Error;
		return false;
	}
	g_sdStorageState = SdStorageState::Ready;
	{
		SdGuard guard;
		if (!SD.exists("/playlists")) {
			SD.mkdir("/playlists");
		}
	}
	performStorageBootCleanup();
	refreshSdStatsDirect();
	return true;
}

inline uint64_t getSdTotalBytes() {
	refreshSdStatsIfNeeded();
	return g_cachedTotalBytes;
}

inline uint64_t getSdUsedBytes() {
	refreshSdStatsIfNeeded();
	return g_cachedUsedBytes;
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

	if (lower.length() == 0) {
		return true;
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

inline bool isSafeUploadFilename(const String &name) {
	if (name.length() == 0 || name.length() > 96)
		return false;
	if (name.startsWith("."))
		return false;

	String lower = name;
	lower.toLowerCase();
	for (size_t i = 0; i < NUM_HIDDEN_EXTENSIONS; i++) {
		if (lower.endsWith(HIDDEN_EXTENSIONS[i])) {
			return false;
		}
	}
	for (size_t i = 0; i < NUM_HIDDEN_EXACT_NAMES; i++) {
		if (lower == HIDDEN_EXACT_NAMES[i]) {
			return false;
		}
	}

	for (size_t i = 0; i < name.length(); i++) {
		char c = name[i];
		if ((uint8_t)c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
			c == '"' || c == '<' || c == '>' || c == '|') {
			return false;
		}
	}
	return true;
}

inline bool isSafePath(const String &path) {
	if (path.length() == 0 || path.length() > 128)
		return false;
	if (!path.startsWith("/"))
		return false;
	if (path.indexOf("..") != -1)
		return false;
	if (path.indexOf('\\') != -1)
		return false;
	return true;
}

inline bool isSafePlaylistName(const String &name) {
	if (name.length() == 0 || name.length() > 64)
		return false;
	if (name.indexOf('/') != -1 || name.indexOf('\\') != -1 || name.indexOf("..") != -1)
		return false;
	String lower = name;
	lower.toLowerCase();
	if (lower.endsWith(".m3u")) {
		lower = lower.substring(0, lower.length() - 4);
	}
	if (lower.length() == 0)
		return false;
	return true;
}

inline String getPlaylistPath(String name) {
	if (!name.endsWith(".m3u"))
		name += ".m3u";
	return "/playlists/" + name;
}

inline bool canMutateStoragePathUnderGuard(const String &path) {
	if (g_sdStorageState == SdStorageState::Error)
		return false;
	if (!isSafePath(path))
		return false;
	if (path == "/" || path == "/playlists" || path == "/playlists/")
		return false;
	if (isProtectedStoragePath(path))
		return false;
	if (isAudioPathCurrentlyActive(path))
		return false;
	return true;
}

inline bool atomicReplaceFile(const String &tempPath, const String &targetPath) {
	if (!isSdReady())
		return false;
	setSdStorageState(SdStorageState::Busy);
	if (SD.exists(targetPath)) {
		SD.remove(targetPath);
	}
	bool ok = SD.rename(tempPath, targetPath);
	setSdStorageState(ok ? SdStorageState::Ready : SdStorageState::Error);
	return ok;
}

inline bool deleteStoragePathInternal(const String &path) {
	if (!canMutateStoragePathUnderGuard(path)) {
		return false;
	}
	if (!SD.exists(path)) {
		return false;
	}
	File target = SD.open(path);
	if (!target) {
		return false;
	}
	if (!target.isDirectory()) {
		target.close();
		return SD.remove(path);
	}
	File entry = target.openNextFile();
	while (entry) {
		String childName = entry.name();
		String childPath = (path == "/" ? "" : path) + "/" + childName;
		bool isDir = entry.isDirectory();
		entry.close();
		if (isDir) {
			deleteStoragePathInternal(childPath);
		} else {
			if (canMutateStoragePathUnderGuard(childPath)) {
				SD.remove(childPath);
			}
		}
		entry = target.openNextFile();
	}
	target.close();
	return SD.rmdir(path);
}

inline bool deleteStoragePath(const String &path) {
	if (!isSdReady())
		return false;
	setSdStorageState(SdStorageState::Busy);
	SdGuard guard;
	bool ok = deleteStoragePathInternal(path);
	if (ok) {
		refreshSdStatsDirect();
		setSdStorageState(SdStorageState::Ready);
	} else {
		setSdStorageState(SdStorageState::Error);
	}
	return ok;
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
	if (g_trackCacheValid || !isSdReady())
		return;

	std::vector<TrackInfo> fresh;
	fresh.reserve(64);
	std::vector<String> dirsToScan;
	dirsToScan.push_back("/");

	while (!dirsToScan.empty() && fresh.size() < MAX_TRACKS_CACHE) {
		String currentDir = dirsToScan.back();
		dirsToScan.pop_back();

		std::vector<String> localSubDirs;
		std::vector<TrackInfo> localTracks;

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
							localSubDirs.push_back(fullPath);
						} else if (isMp3(name)) {
							localTracks.push_back({name, fullPath, entry.size()});
						}
					}
					entry.close();
				}
				dir.close();
			}
		}

		for (const auto &t : localTracks) {
			if (fresh.size() < MAX_TRACKS_CACHE) {
				fresh.push_back(t);
			}
		}
		for (const auto &sd : localSubDirs) {
			dirsToScan.push_back(sd);
		}

		vTaskDelay(pdMS_TO_TICKS(2));
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
	if (g_playlistCacheValid || !isSdReady())
		return;

	std::vector<String> playlistFiles;
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
					if (playlistFiles.size() < MAX_PLAYLISTS_CACHE) {
						playlistFiles.push_back(name);
					}
				}
				entry.close();
			}
			dir.close();
		}
	}

	std::vector<PlaylistInfo> freshPlaylists;
	std::map<String, std::vector<String>> freshIndex;

	for (const String &name : playlistFiles) {
		String fullPath = "/playlists/" + name;
		String base = name;
		base.replace(".m3u", "");
		int count = 0;
		std::vector<String> lines;

		{
			SdGuard guard;
			File f = SD.open(fullPath, FILE_READ);
			if (f) {
				while (f.available()) {
					String line = f.readStringUntil('\n');
					line.trim();
					if (line.length() > 0 && !line.startsWith("#")) {
						lines.push_back(line);
					}
				}
				f.close();
			}
		}

		count = (int)lines.size();
		for (const String &line : lines) {
			if (freshIndex[line].size() < 16) {
				freshIndex[line].push_back(base);
			}
		}
		freshPlaylists.push_back({name, count});
		vTaskDelay(pdMS_TO_TICKS(2));
	}

	g_playlistCache = freshPlaylists;
	g_trackPlaylistIndex = freshIndex;
	g_playlistCacheValid = true;
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

inline std::vector<String> getPlaylistTrackList(const String &name) {
	String path = getPlaylistPath(name);
	std::vector<String> lines;
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return lines;
	while (f.available()) {
		String trackPath = f.readStringUntil('\n');
		trackPath.trim();
		if (trackPath.length() > 0 && !trackPath.startsWith("#")) {
			lines.push_back(trackPath);
		}
	}
	f.close();
	return lines;
}

inline String getPlaylistFirstTrack(const String &name) {
	String path = getPlaylistPath(name);
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return "";
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() > 0 && !line.startsWith("#")) {
			f.close();
			return line;
		}
	}
	f.close();
	return "";
}

inline bool createPlaylist(const String &name) {
	if (!isSdReady())
		return false;
	setSdStorageState(SdStorageState::Busy);
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (SD.exists(path)) {
		setSdStorageState(SdStorageState::Ready);
		return false;
	}
	File f = SD.open(path, FILE_WRITE);
	if (!f) {
		setSdStorageState(SdStorageState::Error);
		return false;
	}
	f.close();
	refreshSdStatsDirect();
	setSdStorageState(SdStorageState::Ready);
	return true;
}

inline bool renamePlaylist(const String &oldName, const String &newName) {
	if (!isSdReady())
		return false;
	setSdStorageState(SdStorageState::Busy);
	SdGuard guard;
	String oldPath = getPlaylistPath(oldName);
	String newPath = getPlaylistPath(newName);
	if (!SD.exists(oldPath) || SD.exists(newPath)) {
		setSdStorageState(SdStorageState::Ready);
		return false;
	}
	bool ok = SD.rename(oldPath, newPath);
	setSdStorageState(ok ? SdStorageState::Ready : SdStorageState::Error);
	return ok;
}

inline bool deletePlaylist(const String &name) {
	if (!isSdReady())
		return false;
	setSdStorageState(SdStorageState::Busy);
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (!SD.exists(path)) {
		setSdStorageState(SdStorageState::Ready);
		return false;
	}
	bool ok = SD.remove(path);
	if (ok) {
		refreshSdStatsDirect();
		setSdStorageState(SdStorageState::Ready);
	} else {
		setSdStorageState(SdStorageState::Error);
	}
	return ok;
}

inline bool appendPlaylistTrack(const String &name, const String &trackPath) {
	if (!isSdReady())
		return false;
	setSdStorageState(SdStorageState::Busy);
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (!SD.exists(path) || !isSafePath(trackPath) || !isMp3(trackPath)) {
		setSdStorageState(SdStorageState::Ready);
		return false;
	}
	if (!SD.exists(trackPath)) {
		setSdStorageState(SdStorageState::Ready);
		return false;
	}
	File countF = SD.open(path, FILE_READ);
	if (!countF) {
		setSdStorageState(SdStorageState::Error);
		return false;
	}
	size_t currentTracks = 0;
	while (countF.available()) {
		String l = countF.readStringUntil('\n');
		l.trim();
		if (l.length() > 0 && !l.startsWith("#")) {
			currentTracks++;
		}
	}
	countF.close();
	if (currentTracks >= MAX_PLAYLIST_TRACKS) {
		setSdStorageState(SdStorageState::Ready);
		return false;
	}

	File f = SD.open(path, FILE_APPEND);
	if (!f) {
		setSdStorageState(SdStorageState::Error);
		return false;
	}
	size_t written = f.println(trackPath);
	f.close();
	if (written > 0) {
		setSdStorageState(SdStorageState::Ready);
		return true;
	}
	setSdStorageState(SdStorageState::Error);
	return false;
}

inline bool overwritePlaylistTracks(const String &name, const std::vector<String> &tracks) {
	if (!isSdReady() || tracks.size() > MAX_PLAYLIST_TRACKS)
		return false;
	setSdStorageState(SdStorageState::Busy);
	SdGuard guard;
	String path = getPlaylistPath(name);
	if (SD.exists(path)) {
		SD.remove(path);
	}
	File f = SD.open(path, FILE_WRITE);
	if (!f) {
		setSdStorageState(SdStorageState::Error);
		return false;
	}
	for (const String &tp : tracks) {
		f.println(tp);
	}
	f.close();
	setSdStorageState(SdStorageState::Ready);
	return true;
}
