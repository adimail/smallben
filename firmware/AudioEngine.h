#pragma once
#include "Config.h"
#include <Arduino.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <SD.h>

static AudioGeneratorMP3 *audioMp3 = nullptr;
static AudioFileSourceSD *audioFile = nullptr;
static AudioOutputI2S *audioOut = nullptr;

static bool audioIsPlaying = false;
static bool audioIsPaused = false;
static String audioCurrentPath = "";

inline void stopAudio() {
	if (audioMp3) {
		if (audioMp3->isRunning()) {
			audioMp3->stop();
		}
		delete audioMp3;
		audioMp3 = nullptr;
	}
	if (audioFile) {
		audioFile->close();
		delete audioFile;
		audioFile = nullptr;
	}
	audioIsPlaying = false;
	audioIsPaused = false;
	audioCurrentPath = "";
}

inline void initAudio() {
	audioOut = new AudioOutputI2S();
	audioOut->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
	audioOut->SetGain(0.85);
}

inline bool startTrack(const String &path) {
	stopAudio();
	if (!SD.exists(path)) {
		return false;
	}
	audioFile = new AudioFileSourceSD(path.c_str());
	audioMp3 = new AudioGeneratorMP3();
	if (!audioMp3->begin(audioFile, audioOut)) {
		stopAudio();
		return false;
	}
	audioIsPlaying = true;
	audioIsPaused = false;
	audioCurrentPath = path;
	return true;
}

inline bool togglePlayAudio(const String &path) {
	if (audioIsPlaying && audioCurrentPath == path) {
		audioIsPaused = !audioIsPaused;
		return true;
	}
	return startTrack(path);
}

inline void handleAudio() {
	if (audioMp3 && audioMp3->isRunning() && !audioIsPaused) {
		if (!audioMp3->loop()) {
			stopAudio();
		}
	}
}

inline String getAudioState() {
	if (!audioIsPlaying)
		return "stopped";
	if (audioIsPaused)
		return "paused";
	return "playing";
}

inline String getAudioPath() { return audioCurrentPath; }
