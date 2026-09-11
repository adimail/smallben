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

enum class AudioCmdType { Play, Stop, Toggle };

struct AudioCommand {
	AudioCmdType type;
	char path[128];
};

struct AudioStateSnapshot {
	bool isPlaying;
	bool isPaused;
	char path[128];
};

static AudioGeneratorMP3 *audioMp3 = nullptr;
static AudioFileSourceSD *audioFile = nullptr;
static AudioOutputI2S *audioOut = nullptr;

static bool audioIsPlaying = false;
static bool audioIsPaused = false;
static char audioCurrentPathBuf[128] = {0};

static SemaphoreHandle_t audioStateMutex = nullptr;
static QueueHandle_t audioCommandQueue = nullptr;
static TaskHandle_t audioTaskHandle = nullptr;

inline void ensureAudioStateMutex() {
	if (!audioStateMutex) {
		audioStateMutex = xSemaphoreCreateMutex();
	}
}

inline AudioStateSnapshot getAudioSnapshot() {
	ensureAudioStateMutex();
	AudioStateSnapshot snap;
	snap.isPlaying = false;
	snap.isPaused = false;
	snap.path[0] = '\0';
	if (audioStateMutex && xSemaphoreTake(audioStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
		snap.isPlaying = audioIsPlaying;
		snap.isPaused = audioIsPaused;
		strncpy(snap.path, audioCurrentPathBuf, sizeof(snap.path) - 1);
		snap.path[sizeof(snap.path) - 1] = '\0';
		xSemaphoreGive(audioStateMutex);
	}
	return snap;
}

inline void updateAudioState(bool playing, bool paused, const char *path) {
	ensureAudioStateMutex();
	if (audioStateMutex && xSemaphoreTake(audioStateMutex, portMAX_DELAY) == pdTRUE) {
		audioIsPlaying = playing;
		audioIsPaused = paused;
		if (path) {
			strncpy(audioCurrentPathBuf, path, sizeof(audioCurrentPathBuf) - 1);
			audioCurrentPathBuf[sizeof(audioCurrentPathBuf) - 1] = '\0';
		} else {
			audioCurrentPathBuf[0] = '\0';
		}
		xSemaphoreGive(audioStateMutex);
	}
}

inline bool isAudioPathActive(const String &targetPath) {
	AudioStateSnapshot snap = getAudioSnapshot();
	if (!snap.isPlaying && !snap.isPaused) {
		return false;
	}
	String current(snap.path);
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

inline void stopAudioInternal() {
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
	updateAudioState(false, false, "");
	clearAudioPathReserved();
}

inline void releaseAudioI2S() {
	stopAudioInternal();
	if (audioOut) {
		delete audioOut;
		audioOut = nullptr;
	}
}

inline void reclaimAudioI2S() {
	if (!audioOut) {
		audioOut = new AudioOutputI2S();
		if (audioOut) {
			audioOut->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
			audioOut->SetGain(0.85);
		}
	}
}

inline bool startTrackInternal(const char *path) {
	stopAudioInternal();
	if (!audioOut) {
		return false;
	}
	if (!path || strlen(path) == 0) {
		return false;
	}
	String p(path);
	if (!isSafePath(p) || !isMp3(p)) {
		return false;
	}

	SdGuard sdGuard;
	if (!SD.exists(p)) {
		return false;
	}

	setAudioPathReserved(path);

	audioFile = new AudioFileSourceSD(path);
	if (!audioFile) {
		clearAudioPathReserved();
		return false;
	}
	audioMp3 = new AudioGeneratorMP3();
	if (!audioMp3) {
		delete audioFile;
		audioFile = nullptr;
		clearAudioPathReserved();
		return false;
	}
	if (!audioMp3->begin(audioFile, audioOut)) {
		audioMp3->stop();
		delete audioMp3;
		audioMp3 = nullptr;
		audioFile->close();
		delete audioFile;
		audioFile = nullptr;
		clearAudioPathReserved();
		return false;
	}

	updateAudioState(true, false, path);
	clearAudioPathReserved();
	return true;
}

inline void togglePlayInternal(const char *path) {
	if (!audioOut) {
		return;
	}
	AudioStateSnapshot snap = getAudioSnapshot();
	if (snap.isPlaying && strcmp(snap.path, path) == 0) {
		bool newPaused = !snap.isPaused;
		if (newPaused) {
			if (audioOut)
				audioOut->stop();
		} else {
			if (audioOut)
				audioOut->begin();
		}
		updateAudioState(true, newPaused, snap.path);
		return;
	}
	startTrackInternal(path);
}

inline void handleAudioInternal() {
	AudioStateSnapshot snap = getAudioSnapshot();
	if (audioMp3 && audioMp3->isRunning() && !snap.isPaused) {
		bool ok = false;
		{
			SdGuard sdGuard;
			ok = audioMp3->loop();
		}
		if (!ok) {
			stopAudioInternal();
		}
	}
}

inline void audioTaskLoop(void *param) {
	AudioCommand cmd;
	for (;;) {
		while (audioCommandQueue && xQueueReceive(audioCommandQueue, &cmd, 0) == pdTRUE) {
			switch (cmd.type) {
			case AudioCmdType::Play:
				startTrackInternal(cmd.path);
				break;
			case AudioCmdType::Stop:
				stopAudioInternal();
				break;
			case AudioCmdType::Toggle:
				togglePlayInternal(cmd.path);
				break;
			}
		}
		handleAudioInternal();
		vTaskDelay(AUDIO_TASK_DELAY_TICKS);
	}
}

inline bool requestAudioPlay(const String &path) {
	if (!audioCommandQueue || !isSafePath(path) || !isMp3(path))
		return false;
	AudioCommand cmd;
	cmd.type = AudioCmdType::Play;
	strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
	cmd.path[sizeof(cmd.path) - 1] = '\0';
	return (xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE);
}

inline bool requestAudioStop() {
	if (!audioCommandQueue)
		return false;
	AudioCommand cmd;
	cmd.type = AudioCmdType::Stop;
	cmd.path[0] = '\0';
	return (xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE);
}

inline bool requestAudioToggle(const String &path) {
	if (!audioCommandQueue || !isSafePath(path) || !isMp3(path))
		return false;
	AudioCommand cmd;
	cmd.type = AudioCmdType::Toggle;
	strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
	cmd.path[sizeof(cmd.path) - 1] = '\0';
	return (xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE);
}

inline bool initAudio() {
	ensureAudioStateMutex();
	if (!audioStateMutex) {
		return false;
	}
	if (!audioCommandQueue) {
		audioCommandQueue = xQueueCreate(16, sizeof(AudioCommand));
		if (!audioCommandQueue) {
			return false;
		}
	}
	reclaimAudioI2S();
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
