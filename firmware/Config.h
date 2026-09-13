#pragma once
#include <Arduino.h>

#define SD_CS_PIN 5
#define SD_SCK_PIN 18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

#define I2S_BCLK_PIN 27
#define I2S_LRC_PIN 26
#define I2S_DIN_PIN 25

#define RFID_SS_PIN 21
#define RFID_RST_PIN 22
#define RFID_SCK_PIN 14
#define RFID_MISO_PIN 4
#define RFID_MOSI_PIN 13

#define WIFI_BUTTON_PIN 32
#define WIFI_LED_PIN 33

#define MODE_SWITCH_PIN 16
#define VOLUME_POT_PIN 34

const char *AP_SSID = "SmallBen";
const char *AP_PASS_DEFAULT = "smallben123";
const uint16_t HTTP_PORT = 80;

const char *BT_DEVICE_NAME = "Smallben";

const size_t MAX_UPLOAD_SIZE = 31457280;
const size_t MIN_SD_FREE_MARGIN = 2097152;
const size_t MAX_JSON_BODY_SIZE = 16384;

const char *const HIDDEN_EXACT_NAMES[] = {"playlists",	  "system volume information",
										  "thumbs.db",	  ".ds_store",
										  "discs.csv",	  "discs.tmp",
										  "discs.new",	  "discs.bak",
										  "discs.csv.bak"};
const size_t NUM_HIDDEN_EXACT_NAMES = sizeof(HIDDEN_EXACT_NAMES) / sizeof(HIDDEN_EXACT_NAMES[0]);

const char *const HIDDEN_EXTENSIONS[] = {".m3u", ".db",	 ".dat",	   ".idx", ".bin",
										 ".sys", ".tmp", ".uploading", ".bak", ".new"};
const size_t NUM_HIDDEN_EXTENSIONS = sizeof(HIDDEN_EXTENSIONS) / sizeof(HIDDEN_EXTENSIONS[0]);

const char *const HIDDEN_PREFIXES[] = {".", "._"};
const size_t NUM_HIDDEN_PREFIXES = sizeof(HIDDEN_PREFIXES) / sizeof(HIDDEN_PREFIXES[0]);

const uint32_t AUDIO_TASK_STACK_SIZE = 4096;
const UBaseType_t AUDIO_TASK_PRIORITY = 4;
const BaseType_t AUDIO_TASK_CORE = 1;
const TickType_t AUDIO_TASK_DELAY_TICKS = 1;
const int I2S_DMA_BUFFER_COUNT = 8;

const size_t MAX_PLAYLIST_TRACKS = 128;

inline void heapLog(const char *tag) {
	Serial.printf("[HEAP] %-28s free=%u maxalloc=%u minever=%u psram=%u\n", tag, ESP.getFreeHeap(),
				  ESP.getMaxAllocHeap(), ESP.getMinFreeHeap(), ESP.getPsramSize());
}
