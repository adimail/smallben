#pragma once
#include "Config.h"
#include "Storage.h"
#include <Arduino.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <vector>

enum class AudioCmdType {
	Play,
	PlayOnce,
	PlayPlaylist,
	Stop,
	Toggle,
	ReleaseI2S,
	ReclaimI2S,
	SetVolume
};

struct AudioCommand {
	AudioCmdType type;
	char path[128];
	float gain;
};

struct AudioState {
	bool isPlaying;
	bool isPaused;
	bool isStarting;
	char path[128];
};

static AudioGeneratorMP3 *audioMp3 = nullptr;
static AudioFileSourceSD *audioFile = nullptr;
static AudioOutputI2S *audioOut = nullptr;

static AudioState audioState = {false, false, false, {0}};
static float currentAudioGain = 0.85f;

static bool audioIsPlaylist = false;
static char audioCurrentPlaylistName[64] = {0};
static size_t audioCurrentPlaylistIndex = 0;
static char audioCurrentTrack[128] = {0};
static bool audioCurrentTrackLoop = true;
static std::vector<String> audioPlaylistTracks;

static SemaphoreHandle_t audioStateMutex = nullptr;
static SemaphoreHandle_t audioSyncSem = nullptr;
static QueueHandle_t audioCommandQueue = nullptr;
static TaskHandle_t audioTaskHandle = nullptr;

inline void ensureAudioStateMutex() {
	if (!audioStateMutex) {
		audioStateMutex = xSemaphoreCreateMutex();
	}
	if (!audioSyncSem) {
		audioSyncSem = xSemaphoreCreateBinary();
	}
}

inline bool isAudioReady() { return (audioCommandQueue != nullptr && audioTaskHandle != nullptr); }

inline AudioState getAudioState() {
	ensureAudioStateMutex();
	AudioState snap = {false, false, false, {0}};
	if (audioStateMutex && xSemaphoreTake(audioStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
		snap = audioState;
		xSemaphoreGive(audioStateMutex);
	}
	return snap;
}

inline void updateAudioState(bool playing, bool paused, bool starting, const char *path) {
	ensureAudioStateMutex();
	if (audioStateMutex && xSemaphoreTake(audioStateMutex, portMAX_DELAY) == pdTRUE) {
		audioState.isPlaying = playing;
		audioState.isPaused = paused;
		audioState.isStarting = starting;
		if (path) {
			strncpy(audioState.path, path, sizeof(audioState.path) - 1);
			audioState.path[sizeof(audioState.path) - 1] = '\0';
		} else {
			audioState.path[0] = '\0';
		}
		xSemaphoreGive(audioStateMutex);
	}
}

inline bool isAudioPathActive(const String &targetPath) {
	AudioState state = getAudioState();
	if (!state.isPlaying && !state.isPaused && !state.isStarting) {
		return false;
	}
	String current(state.path);
	if (current.length() == 0) {
		return false;
	}
	if (current == targetPath) {
		return true;
	}
	if (targetPath != "/" && current.startsWith(targetPath + "/")) {
		return true;
	}
	return false;
}

inline void stopPlaybackInternal() {
	if (audioMp3) {
		if (audioMp3->isRunning()) {
			SdGuard sdGuard;
			audioMp3->stop();
		}
		delete audioMp3;
		audioMp3 = nullptr;
	}
	if (audioFile) {
		SdGuard sdGuard;
		audioFile->close();
		delete audioFile;
		audioFile = nullptr;
	}
	if (audioOut) {
		audioOut->stop();
	}
	updateAudioState(false, false, false, "");
}

inline void stopAudioInternal() {
	stopPlaybackInternal();
	audioIsPlaylist = false;
	audioCurrentPlaylistName[0] = '\0';
	audioCurrentPlaylistIndex = 0;
	audioCurrentTrack[0] = '\0';
	audioCurrentTrackLoop = false;
	audioPlaylistTracks.clear();
}

inline void releaseAudioI2SInternal() {
	stopAudioInternal();
	if (audioOut) {
		delete audioOut;
		audioOut = nullptr;
	}
}

inline bool reclaimAudioI2SInternal() {
	if (!audioOut) {
		audioOut = new AudioOutputI2S(0, 0, I2S_DMA_BUFFER_COUNT);
		if (!audioOut) {
			return false;
		}
		audioOut->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
		audioOut->SetGain(currentAudioGain);
	}
	return true;
}

inline bool startTrackInternal(const char *path) {
	heapLog("startTrackInternal:enter");
	stopPlaybackInternal();
	if (!audioOut) {
		return false;
	}
	if (!path || strlen(path) == 0) {
		return false;
	}
	String p(path);
	if (!isValidPath(p) || !isMp3(p)) {
		return false;
	}

	SdGuard sdGuard;
	if (!SD.exists(p)) {
		return false;
	}

	updateAudioState(false, false, true, path);

	audioFile = new AudioFileSourceSD(path);
	heapLog("startTrackInternal:after new AudioFileSourceSD");
	if (!audioFile) {
		updateAudioState(false, false, false, "");
		return false;
	}
	audioMp3 = new AudioGeneratorMP3();
	heapLog("startTrackInternal:after new AudioGeneratorMP3");
	if (!audioMp3) {
		delete audioFile;
		audioFile = nullptr;
		updateAudioState(false, false, false, "");
		return false;
	}
	if (!audioMp3->begin(audioFile, audioOut)) {
		heapLog("startTrackInternal:mp3 begin FAILED");
		audioMp3->stop();
		delete audioMp3;
		audioMp3 = nullptr;
		audioFile->close();
		delete audioFile;
		audioFile = nullptr;
		updateAudioState(false, false, false, "");
		return false;
	}
	heapLog("startTrackInternal:after mp3 begin (playing)");

	strncpy(audioCurrentTrack, path, sizeof(audioCurrentTrack) - 1);
	audioCurrentTrack[sizeof(audioCurrentTrack) - 1] = '\0';
	updateAudioState(true, false, false, path);
	return true;
}

inline bool startPlaylistInternal(const char *name) {
	heapLog("startPlaylistInternal:enter");
	stopAudioInternal();
	if (!name || strlen(name) == 0) {
		return false;
	}
	String pName(name);
	if (!isValidPlaylistName(pName)) {
		return false;
	}
	audioPlaylistTracks.clear();
	if (!loadPlaylistTracks(pName, audioPlaylistTracks) || audioPlaylistTracks.empty()) {
		return false;
	}
	audioIsPlaylist = true;
	strncpy(audioCurrentPlaylistName, name, sizeof(audioCurrentPlaylistName) - 1);
	audioCurrentPlaylistName[sizeof(audioCurrentPlaylistName) - 1] = '\0';
	audioCurrentPlaylistIndex = 0;

	size_t count = audioPlaylistTracks.size();
	size_t attempts = 0;
	while (attempts < count) {
		const String &trackPath = audioPlaylistTracks[audioCurrentPlaylistIndex];
		if (startTrackInternal(trackPath.c_str())) {
			heapLog("startPlaylistInternal:track started");
			return true;
		}
		audioCurrentPlaylistIndex = (audioCurrentPlaylistIndex + 1) % count;
		attempts++;
	}
	stopAudioInternal();
	return false;
}

inline void onTrackFinishedInternal() {
	heapLog("onTrackFinishedInternal:enter");
	if (audioIsPlaylist && !audioPlaylistTracks.empty()) {
		size_t count = audioPlaylistTracks.size();
		audioCurrentPlaylistIndex = (audioCurrentPlaylistIndex + 1) % count;
		size_t attempts = 0;
		bool started = false;
		while (attempts < count) {
			const String &nextTrack = audioPlaylistTracks[audioCurrentPlaylistIndex];
			if (startTrackInternal(nextTrack.c_str())) {
				started = true;
				break;
			}
			audioCurrentPlaylistIndex = (audioCurrentPlaylistIndex + 1) % count;
			attempts++;
		}
		if (!started) {
			stopAudioInternal();
		}
	} else if (strlen(audioCurrentTrack) > 0 && audioCurrentTrackLoop) {
		if (!startTrackInternal(audioCurrentTrack)) {
			stopAudioInternal();
		}
	} else {
		stopAudioInternal();
	}
}

inline void togglePlayInternal(const char *path) {
	if (!audioOut) {
		return;
	}
	AudioState state = getAudioState();
	if (path && strlen(path) > 0) {
		if (state.isPlaying && strcmp(state.path, path) == 0) {
			bool newPaused = !state.isPaused;
			if (newPaused) {
				audioOut->stop();
			} else {
				if (!audioOut->begin()) {
					stopAudioInternal();
					return;
				}
			}
			updateAudioState(true, newPaused, false, state.path);
			return;
		}
		audioIsPlaylist = false;
		audioCurrentPlaylistName[0] = '\0';
		audioCurrentPlaylistIndex = 0;
		audioCurrentTrackLoop = true;
		startTrackInternal(path);
		return;
	}
	if (state.isPlaying) {
		bool newPaused = !state.isPaused;
		if (newPaused) {
			audioOut->stop();
		} else {
			if (!audioOut->begin()) {
				stopAudioInternal();
				return;
			}
		}
		updateAudioState(true, newPaused, false, state.path);
	}
}

inline void handleAudioInternal() {
	AudioState state = getAudioState();
	if (audioMp3 && audioMp3->isRunning() && !state.isPaused) {
		bool ok = false;
		{
			SdGuard sdGuard;
			ok = audioMp3->loop();
		}
		if (!ok) {
			onTrackFinishedInternal();
		}
	}
}

inline void audioTaskLoop(void *param) {
	AudioCommand cmd;
	for (;;) {
		while (audioCommandQueue && xQueueReceive(audioCommandQueue, &cmd, 0) == pdTRUE) {
			switch (cmd.type) {
			case AudioCmdType::Play:
				audioIsPlaylist = false;
				audioCurrentPlaylistName[0] = '\0';
				audioCurrentPlaylistIndex = 0;
				audioCurrentTrackLoop = true;
				startTrackInternal(cmd.path);
				break;
			case AudioCmdType::PlayOnce:
				audioIsPlaylist = false;
				audioCurrentPlaylistName[0] = '\0';
				audioCurrentPlaylistIndex = 0;
				audioCurrentTrackLoop = false;
				startTrackInternal(cmd.path);
				break;
			case AudioCmdType::PlayPlaylist:
				audioCurrentTrackLoop = false;
				startPlaylistInternal(cmd.path);
				break;
			case AudioCmdType::Stop:
				stopAudioInternal();
				while (xQueuePeek(audioCommandQueue, &cmd, 0) == pdTRUE &&
					   cmd.type == AudioCmdType::Stop) {
					xQueueReceive(audioCommandQueue, &cmd, 0);
				}
				break;
			case AudioCmdType::Toggle:
				togglePlayInternal(cmd.path);
				break;
			case AudioCmdType::ReleaseI2S:
				releaseAudioI2SInternal();
				if (audioSyncSem) {
					xSemaphoreGive(audioSyncSem);
				}
				break;
			case AudioCmdType::ReclaimI2S:
				reclaimAudioI2SInternal();
				if (audioSyncSem) {
					xSemaphoreGive(audioSyncSem);
				}
				break;
			case AudioCmdType::SetVolume:
				currentAudioGain = cmd.gain;
				if (audioOut) {
					audioOut->SetGain(currentAudioGain);
				}
				break;
			}
		}
		handleAudioInternal();
		vTaskDelay(AUDIO_TASK_DELAY_TICKS);
	}
}

inline bool sendAudioCmd(AudioCmdType type, const char *path = nullptr, float gain = 0.0f,
						 TickType_t timeout = pdMS_TO_TICKS(150)) {
	if (!isAudioReady()) {
		return false;
	}
	AudioCommand cmd;
	cmd.type = type;
	cmd.gain = gain;
	if (path) {
		strncpy(cmd.path, path, sizeof(cmd.path) - 1);
		cmd.path[sizeof(cmd.path) - 1] = '\0';
	} else {
		cmd.path[0] = '\0';
	}
	return (xQueueSend(audioCommandQueue, &cmd, timeout) == pdTRUE);
}

inline bool requestAudioPlay(const String &path) {
	if (!isValidPath(path) || !isMp3(path))
		return false;
	return sendAudioCmd(AudioCmdType::Play, path.c_str());
}

inline bool requestAudioPlayOnce(const String &path) {
	if (!isValidPath(path) || !isMp3(path))
		return false;
	return sendAudioCmd(AudioCmdType::PlayOnce, path.c_str());
}

inline bool requestAudioPlayPlaylist(const String &name) {
	if (!isValidPlaylistName(name))
		return false;
	return sendAudioCmd(AudioCmdType::PlayPlaylist, name.c_str());
}

inline bool requestAudioStop() { return sendAudioCmd(AudioCmdType::Stop); }

inline bool requestAudioToggle(const String &path = "") {
	if (path.length() > 0 && (!isValidPath(path) || !isMp3(path)))
		return false;
	return sendAudioCmd(AudioCmdType::Toggle, path.length() > 0 ? path.c_str() : nullptr);
}

inline bool requestAudioVolume(float gain) {
	return sendAudioCmd(AudioCmdType::SetVolume, nullptr, gain, pdMS_TO_TICKS(50));
}

inline bool requestAudioReleaseI2SBlocking() {
	if (!isAudioReady() || !audioSyncSem) {
		return false;
	}
	xSemaphoreTake(audioSyncSem, 0);
	if (!sendAudioCmd(AudioCmdType::ReleaseI2S, nullptr, 0.0f, pdMS_TO_TICKS(200))) {
		return false;
	}
	return (xSemaphoreTake(audioSyncSem, pdMS_TO_TICKS(1500)) == pdTRUE);
}

inline bool requestAudioReclaimI2SBlocking() {
	if (!isAudioReady() || !audioSyncSem) {
		return false;
	}
	xSemaphoreTake(audioSyncSem, 0);
	if (!sendAudioCmd(AudioCmdType::ReclaimI2S, nullptr, 0.0f, pdMS_TO_TICKS(200))) {
		return false;
	}
	return (xSemaphoreTake(audioSyncSem, pdMS_TO_TICKS(1500)) == pdTRUE);
}

inline bool initAudio() {
	ensureAudioStateMutex();
	if (!audioStateMutex || !audioSyncSem) {
		return false;
	}
	if (!audioCommandQueue) {
		audioCommandQueue = xQueueCreate(16, sizeof(AudioCommand));
		if (!audioCommandQueue) {
			return false;
		}
	}
	if (!reclaimAudioI2SInternal()) {
		return false;
	}
	setAudioPathActiveChecker(isAudioPathActive);
	return true;
}

inline bool startAudioTask() {
	if (!audioTaskHandle) {
		BaseType_t res =
			xTaskCreatePinnedToCore(audioTaskLoop, "audioTask", AUDIO_TASK_STACK_SIZE, nullptr,
									AUDIO_TASK_PRIORITY, &audioTaskHandle, AUDIO_TASK_CORE);
		return (res == pdPASS);
	}
	return true;
}
