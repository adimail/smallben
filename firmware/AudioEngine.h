#pragma once
#include "Config.h"
#include "Storage.h"
#include <Arduino.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static AudioGeneratorMP3 *audioMp3 = nullptr;
static AudioFileSourceSD *audioFile = nullptr;
static AudioOutputI2S *audioOut = nullptr;

static bool audioIsPlaying = false;
static bool audioIsPaused = false;
static String audioCurrentPath = "";

static SemaphoreHandle_t audioMutex = nullptr;
static TaskHandle_t audioTaskHandle = nullptr;

class AudioGuard {
  public:
	AudioGuard() { xSemaphoreTakeRecursive(audioMutex, portMAX_DELAY); }
	~AudioGuard() { xSemaphoreGiveRecursive(audioMutex); }
	AudioGuard(const AudioGuard &) = delete;
	AudioGuard &operator=(const AudioGuard &) = delete;
};

inline void stopAudio() {
	AudioGuard guard;
	if (audioMp3) {
		if (audioMp3->isRunning()) {
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
	audioIsPlaying = false;
	audioIsPaused = false;
	audioCurrentPath = "";
}

inline void initAudio() {
	if (!audioMutex) {
		audioMutex = xSemaphoreCreateRecursiveMutex();
	}
	audioOut = new AudioOutputI2S();
	audioOut->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
	audioOut->SetGain(0.85);
}

inline bool startTrack(const String &path) {
	AudioGuard guard;
	stopAudio();
	bool exists;
	{
		SdGuard sdGuard;
		exists = SD.exists(path);
	}
	if (!exists) {
		return false;
	}
	audioFile = new AudioFileSourceSD(path.c_str());
	audioMp3 = new AudioGeneratorMP3();
	bool began;
	{
		SdGuard sdGuard;
		began = audioMp3->begin(audioFile, audioOut);
	}
	if (!began) {
		stopAudio();
		return false;
	}
	audioIsPlaying = true;
	audioIsPaused = false;
	audioCurrentPath = path;
	return true;
}

inline bool togglePlayAudio(const String &path) {
	AudioGuard guard;
	if (audioIsPlaying && audioCurrentPath == path) {
		audioIsPaused = !audioIsPaused;

		if (audioIsPaused) {
			if (audioOut) {
				audioOut->stop();
			}
		} else {
			if (audioOut) {
				audioOut->begin();
			}
		}
		return true;
	}
	return startTrack(path);
}

inline void handleAudio() {
	AudioGuard guard;
	if (audioMp3 && audioMp3->isRunning() && !audioIsPaused) {
		bool ok;
		{
			SdGuard sdGuard;
			ok = audioMp3->loop();
		}
		if (!ok) {
			stopAudio();
		}
	}
}

inline String getAudioState() {
	AudioGuard guard;
	if (!audioIsPlaying)
		return "stopped";
	if (audioIsPaused)
		return "paused";
	return "playing";
}

inline String getAudioPath() {
	AudioGuard guard;
	return audioCurrentPath;
}

inline void audioTaskLoop(void *param) {
	for (;;) {
		handleAudio();
		vTaskDelay(AUDIO_TASK_DELAY_TICKS);
	}
}

inline void startAudioTask() {
	xTaskCreatePinnedToCore(audioTaskLoop, "audioTask", AUDIO_TASK_STACK_SIZE, nullptr,
							AUDIO_TASK_PRIORITY, &audioTaskHandle, AUDIO_TASK_CORE);
}
