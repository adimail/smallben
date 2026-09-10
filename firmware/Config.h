#pragma once
#include <Arduino.h>

#define SD_CS_PIN 5

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
