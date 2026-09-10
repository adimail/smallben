#pragma once
#include <Arduino.h>

#define SD_CS_PIN 5
#define I2S_BCLK_PIN 27
#define I2S_LRC_PIN 26
#define I2S_DIN_PIN 25

const char *AP_SSID = "Smallben-Setup";
const char *AP_PASS = "smallben123";
const uint16_t HTTP_PORT = 80;

const size_t MAX_UPLOAD_SIZE = 31457280;

const char *const HIDDEN_EXACT_NAMES[] = {"playlists", "system volume information", "thumbs.db",
										  ".ds_store"};
const size_t NUM_HIDDEN_EXACT_NAMES = sizeof(HIDDEN_EXACT_NAMES) / sizeof(HIDDEN_EXACT_NAMES[0]);

const char *const HIDDEN_EXTENSIONS[] = {".m3u", ".db", ".dat", ".idx", ".bin", ".sys"};
const size_t NUM_HIDDEN_EXTENSIONS = sizeof(HIDDEN_EXTENSIONS) / sizeof(HIDDEN_EXTENSIONS[0]);

const char *const HIDDEN_PREFIXES[] = {".", "._"};
const size_t NUM_HIDDEN_PREFIXES = sizeof(HIDDEN_PREFIXES) / sizeof(HIDDEN_PREFIXES[0]);

const uint32_t AUDIO_TASK_STACK_SIZE = 8192;
const UBaseType_t AUDIO_TASK_PRIORITY = 3;
const BaseType_t AUDIO_TASK_CORE = 0;
const TickType_t AUDIO_TASK_DELAY_TICKS = 1;
