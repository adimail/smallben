#pragma once
#include "Config.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class StorageResult {
	Ok,
	NotFound,
	AlreadyExists,
	InvalidPath,
	Protected,
	Active,
	NoSpace,
	OpenFailed,
	WriteFailed,
	VerifyFailed,
	RenameFailed,
	DeleteFailed,
	DirectoryNotEmpty,
	StorageUnavailable
};

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

inline bool isStorageReady() { return g_sdReady; }
inline bool isSdReady() { return g_sdReady; }

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

inline String normalizePath(const String &raw) {
	if (raw.length() == 0)
		return "/";
	String p = raw;
	p.replace('\\', '/');
	while (p.indexOf("//") != -1) {
		p.replace("//", "/");
	}
	if (!p.startsWith("/")) {
		p = "/" + p;
	}
	while (p.length() > 1 && p.endsWith("/")) {
		p = p.substring(0, p.length() - 1);
	}
	return p;
}

inline bool isValidPath(const String &path) {
	if (path.length() == 0 || path.length() > 128)
		return false;
	if (path.indexOf('\\') != -1)
		return false;
	if (!path.startsWith("/"))
		return false;
	if (path.indexOf("..") != -1)
		return false;
	return true;
}

inline bool isValidName(const String &name) {
	if (name.length() == 0 || name.length() > 96)
		return false;
	if (name.startsWith("."))
		return false;
	for (size_t i = 0; i < name.length(); i++) {
		char c = name[i];
		if ((uint8_t)c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
			c == '"' || c == '<' || c == '>' || c == '|') {
			return false;
		}
	}
	return true;
}

inline bool isValidPlaylistName(const String &name) {
	if (name.length() == 0 || name.length() > 64)
		return false;
	if (!isValidName(name))
		return false;
	String lower = name;
	lower.toLowerCase();
	if (lower.endsWith(".m3u")) {
		lower = lower.substring(0, lower.length() - 4);
	}
	return lower.length() > 0;
}

inline bool isProtectedPath(const String &path) {
	String p = normalizePath(path);
	if (p == "/" || p == "/p" || p == "/smallben") {
		return true;
	}
	String lower = p;
	lower.toLowerCase();
	if (lower == "/discs.csv" || lower.startsWith("/discs.csv.")) {
		return true;
	}
	int lastSlash = p.lastIndexOf('/');
	String filename = (lastSlash != -1) ? p.substring(lastSlash + 1) : p;
	filename.toLowerCase();
	for (size_t i = 0; i < NUM_HIDDEN_PREFIXES; i++) {
		if (filename.startsWith(HIDDEN_PREFIXES[i])) {
			return true;
		}
	}
	for (size_t i = 0; i < NUM_HIDDEN_EXACT_NAMES; i++) {
		if (filename == HIDDEN_EXACT_NAMES[i]) {
			return true;
		}
	}
	for (size_t i = 0; i < NUM_HIDDEN_EXTENSIONS; i++) {
		if (filename.endsWith(HIDDEN_EXTENSIONS[i])) {
			return true;
		}
	}
	return false;
}

inline bool isSystemOrHidden(const String &name) {
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

inline bool isJunk(String name) { return isSystemOrHidden(name); }
inline bool isSafePath(const String &path) { return isValidPath(path); }
inline bool isSafePlaylistName(const String &name) { return isValidPlaylistName(name); }
inline bool isSafeUploadFilename(const String &name) {
	return isValidName(name) && !isSystemOrHidden(name);
}
inline bool isProtectedStoragePath(const String &path) { return isProtectedPath(path); }

inline bool isMp3(String name) {
	name.toLowerCase();
	return name.endsWith(".mp3");
}

inline String getPlaylistPath(String name) {
	if (!name.endsWith(".m3u")) {
		name += ".m3u";
	}
	return "/p/" + name;
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
	if (!g_sdReady) {
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
	if (!g_sdReady)
		return;
	if (millis() - g_lastSdStatsUpdate > 45000 || g_cachedTotalBytes == 0) {
		refreshSdStatsDirect();
	}
}

inline uint64_t getSdTotalBytes() {
	refreshSdStatsIfNeeded();
	return g_cachedTotalBytes;
}

inline uint64_t getSdUsedBytes() {
	refreshSdStatsIfNeeded();
	return g_cachedUsedBytes;
}

inline uint64_t getSdFreeBytes() {
	refreshSdStatsIfNeeded();
	if (g_cachedTotalBytes > g_cachedUsedBytes) {
		return g_cachedTotalBytes - g_cachedUsedBytes;
	}
	return 0;
}

inline void cleanupTempFiles(File dir, const String &parentPath) {
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
				cleanupTempFiles(childDir, childPath);
				childDir.close();
			}
		} else {
			String lower = name;
			lower.toLowerCase();
			if (lower.endsWith(".tmp") || lower.endsWith(".bak") || lower.endsWith(".uploading")) {
				SD.remove(childPath);
			}
		}
	}
}

inline void performBootCleanup() {
	SdGuard guard;
	File root = SD.open("/");
	if (root) {
		cleanupTempFiles(root, "/");
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
		return false;
	}
	{
		SdGuard guard;
		if (!SD.exists("/p")) {
			if (!SD.mkdir("/p")) {
				g_sdReady = false;
				return false;
			}
		}
	}
	performBootCleanup();
	return true;
}

inline bool storageExists(const String &path) {
	if (!g_sdReady || !isValidPath(path))
		return false;
	String p = normalizePath(path);
	SdGuard guard;
	return SD.exists(p);
}

inline StorageResult createDirectory(const String &path) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String p = normalizePath(path);
	if (!isValidPath(p))
		return StorageResult::InvalidPath;
	if (isProtectedPath(p))
		return StorageResult::Protected;
	SdGuard guard;
	if (SD.exists(p)) {
		return StorageResult::AlreadyExists;
	}
	if (!SD.mkdir(p)) {
		return StorageResult::WriteFailed;
	}
	return StorageResult::Ok;
}

inline StorageResult deleteFile(const String &path) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String p = normalizePath(path);
	if (!isValidPath(p))
		return StorageResult::InvalidPath;
	if (isProtectedPath(p))
		return StorageResult::Protected;
	if (isAudioPathCurrentlyActive(p))
		return StorageResult::Active;

	SdGuard guard;
	if (!SD.exists(p)) {
		return StorageResult::NotFound;
	}
	File f = SD.open(p, FILE_READ);
	if (!f) {
		return StorageResult::NotFound;
	}
	if (f.isDirectory()) {
		f.close();
		return StorageResult::DeleteFailed;
	}
	f.close();
	if (!SD.remove(p)) {
		return StorageResult::DeleteFailed;
	}
	return StorageResult::Ok;
}

inline StorageResult deleteEmptyDirectory(const String &path) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String p = normalizePath(path);
	if (!isValidPath(p))
		return StorageResult::InvalidPath;
	if (p == "/" || isProtectedPath(p))
		return StorageResult::Protected;
	if (isAudioPathCurrentlyActive(p))
		return StorageResult::Active;

	SdGuard guard;
	if (!SD.exists(p)) {
		return StorageResult::NotFound;
	}
	File dir = SD.open(p);
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		return StorageResult::NotFound;
	}
	File entry = dir.openNextFile();
	if (entry) {
		entry.close();
		dir.close();
		return StorageResult::DirectoryNotEmpty;
	}
	dir.close();
	if (!SD.rmdir(p)) {
		return StorageResult::DeleteFailed;
	}
	return StorageResult::Ok;
}

inline StorageResult deleteStoragePath(const String &path) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String p = normalizePath(path);
	if (!isValidPath(p))
		return StorageResult::InvalidPath;
	if (isProtectedPath(p))
		return StorageResult::Protected;
	if (isAudioPathCurrentlyActive(p))
		return StorageResult::Active;

	SdGuard guard;
	if (!SD.exists(p)) {
		return StorageResult::NotFound;
	}
	File target = SD.open(p);
	if (!target) {
		return StorageResult::NotFound;
	}
	bool isDir = target.isDirectory();
	target.close();

	if (isDir) {
		return deleteEmptyDirectory(p);
	}
	return deleteFile(p);
}

inline StorageResult renameStoragePath(const String &oldPath, const String &newPath) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String src = normalizePath(oldPath);
	String dst = normalizePath(newPath);
	if (!isValidPath(src) || !isValidPath(dst))
		return StorageResult::InvalidPath;
	if (isProtectedPath(src) || isProtectedPath(dst))
		return StorageResult::Protected;
	if (isAudioPathCurrentlyActive(src) || isAudioPathCurrentlyActive(dst))
		return StorageResult::Active;

	SdGuard guard;
	if (!SD.exists(src)) {
		return StorageResult::NotFound;
	}
	if (SD.exists(dst)) {
		return StorageResult::AlreadyExists;
	}
	if (!SD.rename(src, dst)) {
		return StorageResult::RenameFailed;
	}
	if (!SD.exists(dst)) {
		return StorageResult::VerifyFailed;
	}
	return StorageResult::Ok;
}

inline StorageResult replaceFileSafelyUnderGuard(const String &tempPath, const String &destPath) {
	String bakPath = destPath + ".bak";
	bool hadDest = SD.exists(destPath);

	if (hadDest) {
		if (SD.exists(bakPath)) {
			SD.remove(bakPath);
		}
		if (!SD.rename(destPath, bakPath)) {
			SD.remove(tempPath);
			return StorageResult::RenameFailed;
		}
	}

	if (!SD.rename(tempPath, destPath)) {
		if (hadDest) {
			SD.rename(bakPath, destPath);
		}
		SD.remove(tempPath);
		return StorageResult::RenameFailed;
	}

	if (hadDest && SD.exists(bakPath)) {
		SD.remove(bakPath);
	}

	if (!SD.exists(destPath)) {
		return StorageResult::VerifyFailed;
	}
	return StorageResult::Ok;
}

inline StorageResult commitUploadFile(const String &tempPath, const String &destPath,
									  size_t expectedSize) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String tmp = normalizePath(tempPath);
	String dst = normalizePath(destPath);
	if (!isValidPath(dst))
		return StorageResult::InvalidPath;
	if (isProtectedPath(dst))
		return StorageResult::Protected;
	if (isAudioPathCurrentlyActive(dst))
		return StorageResult::Active;

	SdGuard guard;
	if (!SD.exists(tmp)) {
		return StorageResult::NotFound;
	}
	File tf = SD.open(tmp, FILE_READ);
	if (!tf) {
		SD.remove(tmp);
		return StorageResult::VerifyFailed;
	}
	size_t actualSize = tf.size();
	tf.close();
	if (actualSize != expectedSize) {
		SD.remove(tmp);
		return StorageResult::VerifyFailed;
	}

	return replaceFileSafelyUnderGuard(tmp, dst);
}

inline StorageResult createPlaylist(const String &name) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	if (!isValidPlaylistName(name))
		return StorageResult::InvalidPath;

	String path = getPlaylistPath(name);
	SdGuard guard;
	if (!SD.exists("/p")) {
		if (!SD.mkdir("/p")) {
			return StorageResult::WriteFailed;
		}
	}
	if (SD.exists(path)) {
		return StorageResult::AlreadyExists;
	}
	File f = SD.open(path, FILE_WRITE);
	if (!f) {
		return StorageResult::OpenFailed;
	}
	f.close();
	if (!SD.exists(path)) {
		return StorageResult::VerifyFailed;
	}
	return StorageResult::Ok;
}

inline StorageResult renamePlaylist(const String &oldName, const String &newName) {
	if (!isValidPlaylistName(oldName) || !isValidPlaylistName(newName)) {
		return StorageResult::InvalidPath;
	}
	return renameStoragePath(getPlaylistPath(oldName), getPlaylistPath(newName));
}

inline StorageResult deletePlaylist(const String &name) {
	if (!isValidPlaylistName(name)) {
		return StorageResult::InvalidPath;
	}
	String path = getPlaylistPath(name);
	return deleteFile(path);
}

inline size_t getPlaylistTrackCount(const String &name) {
	if (!g_sdReady || !isValidPlaylistName(name)) {
		return 0;
	}
	String path = getPlaylistPath(name);
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return 0;
	size_t count = 0;
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

inline bool getPlaylistTrackAt(const String &name, size_t targetIndex, String &outTrack) {
	if (!g_sdReady || !isValidPlaylistName(name)) {
		return false;
	}
	String path = getPlaylistPath(name);
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return false;
	size_t index = 0;
	bool found = false;
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() > 0 && !line.startsWith("#")) {
			if (index == targetIndex) {
				outTrack = line;
				found = true;
				break;
			}
			index++;
		}
	}
	f.close();
	return found;
}

inline StorageResult appendPlaylistTrack(const String &name, const String &trackPath) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	if (!isValidPlaylistName(name))
		return StorageResult::InvalidPath;
	String tp = normalizePath(trackPath);
	if (!isValidPath(tp) || !isMp3(tp))
		return StorageResult::InvalidPath;

	String path = getPlaylistPath(name);
	SdGuard guard;
	if (!SD.exists(path)) {
		return StorageResult::NotFound;
	}
	if (!SD.exists(tp)) {
		return StorageResult::NotFound;
	}

	File f = SD.open(path, FILE_READ);
	if (!f) {
		return StorageResult::OpenFailed;
	}
	size_t count = 0;
	while (f.available()) {
		String l = f.readStringUntil('\n');
		l.trim();
		if (l.length() > 0 && !l.startsWith("#")) {
			count++;
		}
	}
	f.close();

	if (count >= MAX_PLAYLIST_TRACKS) {
		return StorageResult::NoSpace;
	}

	File af = SD.open(path, FILE_APPEND);
	if (!af) {
		return StorageResult::OpenFailed;
	}
	size_t written = af.println(tp);
	af.close();
	if (written == 0) {
		return StorageResult::WriteFailed;
	}
	return StorageResult::Ok;
}

template <typename Callback> inline void iterateDirectory(File dir, Callback cb) {
	while (true) {
		File entry = dir.openNextFile();
		if (!entry)
			break;
		String name = entry.name();
		if (isSystemOrHidden(name)) {
			entry.close();
			continue;
		}
		cb(name, entry.isDirectory(), entry.size());
		entry.close();
	}
}

template <typename Callback> inline bool iteratePlaylistTracks(const String &name, Callback cb) {
	if (!g_sdReady || !isValidPlaylistName(name))
		return false;
	String path = getPlaylistPath(name);
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return false;
	while (f.available()) {
		String trackPath = f.readStringUntil('\n');
		trackPath.trim();
		if (trackPath.length() > 0 && !trackPath.startsWith("#")) {
			int lastSlash = trackPath.lastIndexOf('/');
			String trackName = (lastSlash != -1) ? trackPath.substring(lastSlash + 1) : trackPath;
			size_t sz = 0;
			bool exists = false;
			if (SD.exists(trackPath)) {
				File tf = SD.open(trackPath, FILE_READ);
				if (tf) {
					sz = tf.size();
					exists = true;
					tf.close();
				}
			}
			cb(trackName, trackPath, sz, exists);
		}
	}
	f.close();
	return true;
}
