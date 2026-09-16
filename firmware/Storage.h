#pragma once
#include "Config.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

enum class StorageResult {
	Ok,
	NotFound,
	AlreadyExists,
	InvalidPath,
	Protected,
	Active,
	NoSpace,
	StorageUnavailable,
	Fail
};

struct StorageEntry {
	String name;
	bool isDir;
	size_t size;
	String path;
};

struct PlaylistSummary {
	String name;
	int trackCount;
};

struct PlaylistTrackInfo {
	String name;
	String path;
	size_t size;
	bool exists;
};

struct UploadSession {
	File file;
	String tempPath;
	String finalPath;
};

static SPIClass sdSPI(HSPI);
static SemaphoreHandle_t sdMutex = nullptr;
static bool g_sdReady = false;

static bool (*g_isAudioPathActiveCheck)(const String &path) = nullptr;

inline void ensureSdMutex() {
	if (!sdMutex) {
		sdMutex = xSemaphoreCreateMutex();
	}
}

class SdGuard {
  public:
	SdGuard() {
		ensureSdMutex();
		if (sdMutex) {
			xSemaphoreTake(sdMutex, portMAX_DELAY);
		}
	}
	~SdGuard() {
		if (sdMutex) {
			xSemaphoreGive(sdMutex);
		}
	}
	SdGuard(const SdGuard &) = delete;
	SdGuard &operator=(const SdGuard &) = delete;
};

inline bool isStorageReady() { return g_sdReady; }
inline bool isSdReady() { return g_sdReady; }

inline void setAudioPathActiveChecker(bool (*fn)(const String &)) { g_isAudioPathActiveCheck = fn; }

inline bool isAudioPathCurrentlyActive(const String &path) {
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

inline uint64_t getSdTotalBytes() {
	if (!g_sdReady)
		return 0;
	SdGuard guard;
	return SD.totalBytes();
}

inline uint64_t getSdUsedBytes() {
	if (!g_sdReady)
		return 0;
	SdGuard guard;
	return SD.usedBytes();
}

inline uint64_t getSdFreeBytes() {
	if (!g_sdReady)
		return 0;
	SdGuard guard;
	uint64_t total = SD.totalBytes();
	uint64_t used = SD.usedBytes();
	return (total > used) ? (total - used) : 0;
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
		return StorageResult::Fail;
	}
	return StorageResult::Ok;
}

inline StorageResult deleteFileUnderLock(const String &path) {
	if (!SD.exists(path)) {
		return StorageResult::NotFound;
	}
	File f = SD.open(path, FILE_READ);
	if (!f) {
		return StorageResult::NotFound;
	}
	if (f.isDirectory()) {
		f.close();
		return StorageResult::Fail;
	}
	f.close();
	if (!SD.remove(path)) {
		return StorageResult::Fail;
	}
	return StorageResult::Ok;
}

inline StorageResult deleteEmptyDirectoryUnderLock(const String &path) {
	if (!SD.exists(path)) {
		return StorageResult::NotFound;
	}
	File dir = SD.open(path);
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		return StorageResult::NotFound;
	}
	File entry = dir.openNextFile();
	if (entry) {
		entry.close();
		dir.close();
		return StorageResult::Fail;
	}
	dir.close();
	if (!SD.rmdir(path)) {
		return StorageResult::Fail;
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
		return deleteEmptyDirectoryUnderLock(p);
	}
	return deleteFileUnderLock(p);
}

inline StorageResult replaceFileSafelyUnderLock(const String &tempPath, const String &destPath) {
	if (SD.exists(destPath)) {
		SD.remove(destPath);
	}
	if (!SD.rename(tempPath, destPath)) {
		SD.remove(tempPath);
		return StorageResult::Fail;
	}
	if (!SD.exists(destPath)) {
		return StorageResult::Fail;
	}
	return StorageResult::Ok;
}

inline StorageResult openUploadSession(const String &finalPath, UploadSession &session) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	session.finalPath = finalPath;
	session.tempPath = finalPath + ".tmp";
	SdGuard guard;
	if (SD.exists(session.tempPath))
		SD.remove(session.tempPath);
	session.file = SD.open(session.tempPath, FILE_WRITE);
	if (!session.file)
		return StorageResult::Fail;
	return StorageResult::Ok;
}

inline StorageResult writeUploadSession(UploadSession &session, const uint8_t *data, size_t len) {
	if (!session.file)
		return StorageResult::Fail;
	SdGuard guard;
	size_t written = session.file.write(data, len);
	return (written == len) ? StorageResult::Ok : StorageResult::Fail;
}

inline StorageResult closeUploadSession(UploadSession &session, bool commit) {
	if (session.file) {
		SdGuard guard;
		session.file.close();
	}
	SdGuard guard;
	if (commit) {
		return replaceFileSafelyUnderLock(session.tempPath, session.finalPath);
	} else {
		if (SD.exists(session.tempPath))
			SD.remove(session.tempPath);
		return StorageResult::Ok;
	}
}

inline StorageResult getDirectoryEntries(const String &path, int offset, int limit,
										 std::vector<StorageEntry> &entries, int &totalCount,
										 bool &hasMore) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String p = normalizePath(path);
	if (!isValidPath(p))
		return StorageResult::InvalidPath;

	SdGuard guard;
	if (!SD.exists(p))
		return StorageResult::NotFound;
	File dir = SD.open(p);
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		return StorageResult::NotFound;
	}

	totalCount = 0;
	int collected = 0;
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

		if (totalCount >= offset && collected < limit) {
			String fullPath = (p == "/" ? "" : p) + "/" + ename;
			entries.push_back({ename, edir, esz, fullPath});
			collected++;
		}
		totalCount++;
	}
	dir.close();

	hasMore = (offset + collected) < totalCount;
	return StorageResult::Ok;
}

inline void scanMp3DirectoryRecursive(const String &dirPath, int offset, int limit,
									  std::vector<StorageEntry> &entries, int &currentIndex,
									  int &totalCount, int maxDepth = 4) {
	if (maxDepth <= 0)
		return;
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

		String lower = name;
		lower.toLowerCase();
		if (isSystemOrHidden(name) || lower == "smallben" || lower == "p" || lower == "playlists")
			continue;

		String fullPath = (dirPath == "/" ? "" : dirPath) + "/" + name;
		if (isDir) {
			scanMp3DirectoryRecursive(fullPath, offset, limit, entries, currentIndex, totalCount,
									  maxDepth - 1);
		} else if (isMp3(name)) {
			if (currentIndex >= offset && (int)entries.size() < limit) {
				entries.push_back({name, false, sz, fullPath});
			}
			currentIndex++;
			totalCount++;
		}
	}
	dir.close();
}

inline StorageResult getMp3TracksPaginated(int offset, int limit,
										   std::vector<StorageEntry> &entries, int &totalCount,
										   bool &hasMore) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	SdGuard guard;
	totalCount = 0;
	int currentIndex = 0;
	scanMp3DirectoryRecursive("/", offset, limit, entries, currentIndex, totalCount, 4);
	hasMore = (offset + (int)entries.size()) < totalCount;
	return StorageResult::Ok;
}

inline StorageResult streamFileToClient(const String &path, WiFiClient &client, size_t &fileSize) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	String p = normalizePath(path);
	if (!isValidPath(p) || isProtectedPath(p))
		return StorageResult::InvalidPath;

	SdGuard guard;
	if (!SD.exists(p))
		return StorageResult::NotFound;
	File file = SD.open(p, FILE_READ);
	if (!file || file.isDirectory()) {
		if (file)
			file.close();
		return StorageResult::Fail;
	}

	fileSize = file.size();
	uint8_t buffer[2048];
	size_t bytesSent = 0;

	while (bytesSent < fileSize && client.connected()) {
		size_t toRead = sizeof(buffer);
		if (bytesSent + toRead > fileSize) {
			toRead = fileSize - bytesSent;
		}
		size_t actualRead = file.read(buffer, toRead);
		if (actualRead == 0)
			break;
		size_t written = client.write(buffer, actualRead);
		bytesSent += written;
		if (written != actualRead)
			break;
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	file.close();
	return StorageResult::Ok;
}

inline StorageResult getAllPlaylists(std::vector<PlaylistSummary> &list) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	SdGuard guard;
	File dir = SD.open("/p");
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		return StorageResult::Ok;
	}

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
			String baseName = name.substring(0, name.length() - 4);
			list.push_back({baseName, count});
		}
	}
	dir.close();
	return StorageResult::Ok;
}

inline StorageResult getPlaylistTracks(const String &id, std::vector<PlaylistTrackInfo> &tracks) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	if (!isValidPlaylistName(id))
		return StorageResult::InvalidPath;
	String pPath = getPlaylistPath(id);

	SdGuard guard;
	if (!SD.exists(pPath))
		return StorageResult::NotFound;
	File f = SD.open(pPath, FILE_READ);
	if (!f)
		return StorageResult::NotFound;

	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() > 0 && !line.startsWith("#")) {
			String tPath = line;
			int slash = tPath.lastIndexOf('/');
			String tName = (slash != -1) ? tPath.substring(slash + 1) : tPath;
			size_t sz = 0;
			bool exists = false;
			if (SD.exists(tPath)) {
				File tf = SD.open(tPath, FILE_READ);
				if (tf) {
					sz = tf.size();
					exists = true;
					tf.close();
				}
			}
			tracks.push_back({tName, tPath, sz, exists});
		}
	}
	f.close();
	return StorageResult::Ok;
}

inline StorageResult setPlaylistTracks(const String &id, const std::vector<String> &tracks) {
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	if (!isValidPlaylistName(id))
		return StorageResult::InvalidPath;
	if (tracks.size() > MAX_PLAYLIST_TRACKS)
		return StorageResult::NoSpace;

	String destPath = getPlaylistPath(id);
	String tempPath = destPath + ".tmp";

	SdGuard guard;
	if (SD.exists(tempPath))
		SD.remove(tempPath);
	File outF = SD.open(tempPath, FILE_WRITE);
	if (!outF)
		return StorageResult::Fail;

	for (size_t i = 0; i < tracks.size(); i++) {
		String tp = normalizePath(tracks[i]);
		if (!isValidPath(tp) || !isMp3(tp)) {
			outF.close();
			SD.remove(tempPath);
			return StorageResult::InvalidPath;
		}
		outF.println(tp);
	}
	outF.close();

	return replaceFileSafelyUnderLock(tempPath, destPath);
}

inline std::vector<String> getPlaylistsContainingTrack(const String &trackPath) {
	std::vector<String> matched;
	if (!g_sdReady || !isValidPath(trackPath))
		return matched;

	SdGuard guard;
	File dir = SD.open("/p");
	if (!dir || !dir.isDirectory()) {
		if (dir)
			dir.close();
		return matched;
	}

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
			if (f) {
				while (f.available()) {
					String line = f.readStringUntil('\n');
					line.trim();
					if (line == trackPath) {
						matched.push_back(name.substring(0, name.length() - 4));
						break;
					}
				}
				f.close();
			}
		}
	}
	dir.close();
	return matched;
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
			return StorageResult::Fail;
		}
	}
	if (SD.exists(path)) {
		return StorageResult::AlreadyExists;
	}

	File dir = SD.open("/p");
	if (dir && dir.isDirectory()) {
		size_t pCount = 0;
		while (true) {
			File entry = dir.openNextFile();
			if (!entry)
				break;
			String ename = entry.name();
			bool isDir = entry.isDirectory();
			entry.close();
			String lower = ename;
			lower.toLowerCase();
			if (!isDir && lower.endsWith(".m3u") && !ename.startsWith(".")) {
				pCount++;
			}
		}
		dir.close();
		if (pCount >= MAX_PLAYLISTS) {
			return StorageResult::NoSpace;
		}
	}

	File f = SD.open(path, FILE_WRITE);
	if (!f) {
		return StorageResult::Fail;
	}
	f.close();
	if (!SD.exists(path)) {
		return StorageResult::Fail;
	}
	return StorageResult::Ok;
}

inline void updateDiscsCsvPlaylistRenameUnderLock(const String &oldName, const String &newName) {
	if (!SD.exists("/discs.csv"))
		return;
	File inF = SD.open("/discs.csv", FILE_READ);
	if (!inF)
		return;
	File outF = SD.open("/discs.tmp", FILE_WRITE);
	if (!outF) {
		inF.close();
		return;
	}
	bool changed = false;
	while (inF.available()) {
		String line = inF.readStringUntil('\n');
		line.trim();
		if (line.length() == 0)
			continue;
		int p1 = line.indexOf('|');
		int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
		int p3 = (p2 != -1) ? line.indexOf('|', p2 + 1) : -1;
		if (p1 != -1 && p2 != -1 && p3 != -1) {
			String uid = line.substring(0, p1);
			String type = line.substring(p1 + 1, p2);
			String target = line.substring(p2 + 1, p3);
			String label = line.substring(p3 + 1);
			if (type == "playlist" && target == oldName) {
				target = newName;
				if (label == oldName)
					label = newName;
				changed = true;
			}
			outF.print(uid);
			outF.print('|');
			outF.print(type);
			outF.print('|');
			outF.print(target);
			outF.print('|');
			outF.println(label);
		} else {
			outF.println(line);
		}
	}
	inF.close();
	outF.close();
	if (changed) {
		replaceFileSafelyUnderLock("/discs.tmp", "/discs.csv");
	} else {
		SD.remove("/discs.tmp");
	}
}

inline void updateDiscsCsvPlaylistDeleteUnderLock(const String &name) {
	if (!SD.exists("/discs.csv"))
		return;
	File inF = SD.open("/discs.csv", FILE_READ);
	if (!inF)
		return;
	File outF = SD.open("/discs.tmp", FILE_WRITE);
	if (!outF) {
		inF.close();
		return;
	}
	bool changed = false;
	while (inF.available()) {
		String line = inF.readStringUntil('\n');
		line.trim();
		if (line.length() == 0)
			continue;
		int p1 = line.indexOf('|');
		int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
		int p3 = (p2 != -1) ? line.indexOf('|', p2 + 1) : -1;
		if (p1 != -1 && p2 != -1 && p3 != -1) {
			String type = line.substring(p1 + 1, p2);
			String target = line.substring(p2 + 1, p3);
			if (type == "playlist" && target == name) {
				changed = true;
				continue;
			}
		}
		outF.println(line);
	}
	inF.close();
	outF.close();
	if (changed) {
		replaceFileSafelyUnderLock("/discs.tmp", "/discs.csv");
	} else {
		SD.remove("/discs.tmp");
	}
}

inline StorageResult renamePlaylist(const String &oldName, const String &newName) {
	if (!isValidPlaylistName(oldName) || !isValidPlaylistName(newName)) {
		return StorageResult::InvalidPath;
	}
	String oldPath = getPlaylistPath(oldName);
	String newPath = getPlaylistPath(newName);
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	if (isAudioPathCurrentlyActive(oldPath) || isAudioPathCurrentlyActive(newPath)) {
		return StorageResult::Active;
	}
	SdGuard guard;
	if (!SD.exists(oldPath))
		return StorageResult::NotFound;
	if (SD.exists(newPath))
		return StorageResult::AlreadyExists;
	if (!SD.rename(oldPath, newPath) || !SD.exists(newPath)) {
		return StorageResult::Fail;
	}
	updateDiscsCsvPlaylistRenameUnderLock(oldName, newName);
	return StorageResult::Ok;
}

inline StorageResult deletePlaylist(const String &name) {
	if (!isValidPlaylistName(name)) {
		return StorageResult::InvalidPath;
	}
	String path = getPlaylistPath(name);
	if (!g_sdReady)
		return StorageResult::StorageUnavailable;
	if (isAudioPathCurrentlyActive(path)) {
		return StorageResult::Active;
	}
	SdGuard guard;
	StorageResult res = deleteFileUnderLock(path);
	if (res == StorageResult::Ok) {
		updateDiscsCsvPlaylistDeleteUnderLock(name);
	}
	return res;
}

inline bool loadPlaylistTracks(const String &name, std::vector<String> &tracks) {
	if (!g_sdReady || !isValidPlaylistName(name)) {
		return false;
	}
	String path = getPlaylistPath(name);
	SdGuard guard;
	File f = SD.open(path, FILE_READ);
	if (!f)
		return false;
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.length() > 0 && !line.startsWith("#")) {
			tracks.push_back(line);
		}
	}
	f.close();
	return true;
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
		return StorageResult::Fail;
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
		return StorageResult::Fail;
	}
	size_t written = af.println(tp);
	af.close();
	if (written == 0) {
		return StorageResult::Fail;
	}
	return StorageResult::Ok;
}
