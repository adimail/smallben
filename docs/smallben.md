# Smallben

**Smallben** is a physical, screen-free music player controlled by tapping RFID/NFC "discs" near a reader — inspired by devices like Yoto/Tonieboxes. Each disc is linked to a song or playlist. Bring a disc near the reader, the linked audio plays from an onboard SD card. Take the disc away, playback stops. No app required for day-to-day use; a local Wi-Fi dashboard exists purely for setup — mapping discs, browsing/managing the music library, building playlists, and optional manual playback.

The device also supports a Bluetooth audio mode as an alternative source, and is designed to run on an ESP32 with an SD card, an RFID/NFC reader, and a speaker/amp. The dashboard communicates with the device over **WebSockets**, so status (disc detection, playback state, charging state) updates live without the page needing to poll or refresh.

---

## Core Concept

1. User places an RFID/NFC disc near the reader.
2. Smallben looks up the disc's unique ID in its stored mapping table.
3. If a song or playlist is mapped → playback starts immediately.
4. If nothing is mapped → nothing plays.
5. When the disc is moved out of read range → playback stops immediately.

This tap-to-play/remove-to-stop behavior is the primary interaction. Everything else (Wi-Fi dashboard, Bluetooth mode) is secondary/support functionality.

---

## Modes

Smallben has two **mutually exclusive playback source modes**, controlled by a physical toggle switch, plus an **independent Wi-Fi admin mode** that can be active alongside either.

### 1. Disc Mode (RFID/NFC)
- Default playback mode.
- RFID/NFC reader is active and polling for tags.
- Bluetooth radio is fully off/disconnected while this mode is active.
- Tag present + mapped → plays. Tag present + unmapped → silent. Tag removed → stops.
- **Idle/waiting indication:** while in Disc mode and no disc is currently detected, the disc-status LED slowly blinks, signaling the device is powered on and actively waiting for a disc.
- **Active indication:** as soon as a disc is detected and its mapped song/playlist starts playing, the same LED switches from blinking to solid/steady for as long as playback continues.
- This blink/steady behavior is specific to Disc mode only — it does not apply while the device is in Bluetooth mode.

### 2. Bluetooth Mode
- Device acts as a Bluetooth audio sink (speaker).
- RFID/NFC reading is disabled/ignored — even if a disc is placed near the reader, nothing plays from SD in this mode.
- Switching into this mode disconnects and disables Disc mode; switching back to Disc mode disconnects/kills any active Bluetooth connection.
- The disc-status LED does not blink or track disc presence in this mode, since disc detection is inactive but is always turned on.

### 3. Wi-Fi Portal / Dashboard Mode (independent, not exclusive with the above)
- Triggered by a dedicated **Wi-Fi button**.
- Turns on the ESP32's own Wi-Fi (access point/portal), which a phone or laptop connects to directly in order to reach the local web dashboard.
- Once connected, the dashboard opens a WebSocket connection to the device for live status and control (see Dashboard Features).
- Used for: browsing/managing the SD card's music library, mapping discs to songs/playlists, building playlists, viewing registered discs, manual playback (when allowed), upload songs, delete songs, and seeing live playback status.
- Can be toggled on/off independently of whether the device is currently in Disc mode or Bluetooth mode — it's an admin layer, not an audio source.

---

## Buttons

| Button | Function |
|---|---|
| **Power** | Turns the whole device on/off. |
| **Wi-Fi** | Toggles the Wi-Fi portal/dashboard on/off. |
| **Disc / Bluetooth toggle** | Switches the active playback source between Disc (RFID) mode and Bluetooth mode. Switching one on forces the other off. |
| **Volume** | Adjusts playback volume. *(physical control type — button pair or rotary — TBD)* |

---

## LED Indicators

LEDs communicate device state at a glance, without needing the dashboard open. Planned states:

| LED | Meaning |
|---|---|
| **blue (led is near the button itself)** | Wi-Fi portal is active |
| **Blue — solid** | Bluetooth Connected |
| **Blue — slow blink** | Bluetooth Pairing Mode / Searching |
| **LED strip white - slow blink** | Disc mode active, no disc currently detected (idle/waiting) |
| **LED strip white - solid** | Disc detected & song is playing |
| **LED strip white - double blink** | Disc Detected, but Unmapped / Empty |
| **Red — flash 3x** | Error (Missing SD Card / Unreadable Tag) |
| **Red — solid** | Battery low |
| **orange — solid** | Charger is plugged in and battery is charging |
| **green — solid** | Charger is plugged in and battery is fully charged |

Notes:
- The charging indicator only reflects charger-plugged-in state. It's independent of the mode-related LEDs above (Disc/Bluetooth/Wi-Fi), since charging can happen regardless of playback mode.
- "Low battery" and "charging" indications are mutually exclusive in practice — if the charger is connected, the charging indicator takes priority over the low-battery indicator.

---

## Dashboard Features (Wi-Fi Portal)

The dashboard is reached only when Wi-Fi mode is on and a device connects to Smallben's network. It maintains a live WebSocket connection to the device for real-time updates. It includes:

- **Live disc status** — shows when a disc is detected in real time, and whether it's already mapped.
- **Disc mapping** — tap a detected disc, then choose a song or playlist for it to play; mapping saves instantly to the SD card.
- **Library / full directory browser** — full view of the SD card's folder structure and files (name, file, length, usage count), not just a flat track list.
- **Library management** — add new songs to the SD card (upload) and remove existing songs (delete) directly from the dashboard.
- **Playlists** — view/expand existing playlists, and build new ones from the library.
- **Discs registry** — table of all known discs, what each is mapped to, and when it was last seen.
- **Manual playback controls** — play/pause controls in the dashboard to manually trigger playback of any song/playlist. These controls are only active when **no disc is currently inserted**; if a disc is present, disc-driven playback takes precedence and manual controls are disabled/hidden to avoid conflicting playback sources.
- **Now playing status** — current play/pause state (whether triggered by a disc or manually via the dashboard) is shown live on the dashboard via WebSocket push, and reflected in the page footer whenever something is playing.
- **Charging/battery status** — current battery/charging state is also surfaced on the dashboard, mirroring the physical LED indicator.

---

## Software Architecture (intended structure)

Code is split by responsibility rather than kept in one file, to stay lightweight and maintainable on the ESP32:

- **Main / state machine** — overall device state, mode arbitration (Disc vs Bluetooth), power and LED state.
- **Dashboard / web server** — serves the HTML/JS UI, exposes REST-style endpoints for actions (mapping, playlists, discs, library upload/delete), and runs a **WebSocket server** that pushes live state (disc detection, now-playing, charging status) to connected dashboard clients.
- **Wi-Fi / network** — manages the AP/portal lifecycle (on/off via the Wi-Fi button) and the WebSocket connection lifecycle.
- **Bluetooth** — A2DP sink logic, isolated so it can be cleanly enabled/disabled by mode.
- **SD card / audio** — directory scanning, file listing, file add/remove, playback control (play/pause/stop), arbitration between disc-triggered and dashboard-triggered playback requests.
- **RFID / mapping** — tag detection and the persisted UID → song/playlist mapping table (stored on the SD card).
- **Power / charging** — monitors charging and battery-full state to drive the charging LED indicator and the dashboard's battery status.
- **LEDs** — translates device state (mode, disc presence/playback, Wi-Fi, charging/battery) into LED patterns, including blink timing for idle-waiting and charge-complete states.

Design priorities: minimal memory/flash footprint suited to ESP32 constraints, non-blocking behavior where practical, and safe fallbacks for common failure cases (no SD card present, empty/corrupt mapping data, Wi-Fi failing to start, unreadable tag, missing audio file, WebSocket disconnects/reconnects).

---

### Components

MFRC522 RFID/NFC Reader Module
MicroSD Card Module (SPI breakout)
MAX98357A I2S Class-D Amp Breakout
TP4056 Charging Module (USB C)
MT3608 Step-Up Boost Converter
3.7V 3800mah Li-ion cell
NTAG213 NFC Tags/Stickers
8ohm 1W speaker
leds
esp32
Rotary Mouse Scroll
toggle button
push buttons
