Here is a complete, detailed summary of all the software problems we identified and the corresponding solutions we discussed, grouped by category:

---

### 1. File Upload Architecture & Reliability

* **The SD Free-Space Freeze (`SD.usedBytes()`)**
  * **The Problem:** The moment an upload started, the firmware checked free storage by calling a function that triggered a complete FAT32 file allocation table scan. On large cards (16 GB – 64 GB), scanning every single cluster over SPI takes 3 to 8 seconds. While the ESP32 was frozen doing this scan, the browser was already transmitting data, causing the network receive buffers to overflow and aborting the connection before byte 1 was written.
  * **The Solution:** Remove the full-card cluster scan from the upload startup routine entirely. Only verify that the SD card is mounted and online (or use an infrequently cached value), allowing the file to open immediately without network delay.

* **Tiny, Unbuffered SD Writes vs. Flash Erase Spikes**
  * **The Problem:** Incoming WiFi data arrives in small network packets (~1.4 KB). The code wrote each tiny packet directly to the SD card. An average MP3 required thousands of individual write operations. Every few hundred kilobytes, the SD card must internally erase a flash block or allocate new clusters, which holds the SPI bus busy for up to 400 ms. Because the write call timed out, it returned fewer bytes than expected, triggering the firmware's write-failure check and deleting the file midway through.
  * **The Solution:** Create an in-memory accumulation buffer (e.g., 4 KB, aligned to physical SD flash sectors). Incoming network chunks are gathered in RAM first, and data is only committed to the SD card in full, efficient sector blocks.

* **The Arbitrary 10 MB Ceiling**
  * **The Problem:** The configuration file contained a strict, hardcoded limit of 10 MB. Any file exceeding this size was instantly rejected by the firmware with a "File Too Large" error, regardless of how many gigabytes remained on the SD card.
  * **The Solution:** Remove the hardcoded cap so file size is constrained only by actual available card capacity (keeping a modest safety buffer).

* **Overly Complex Commit Pipeline**
  * **The Problem:** Once an upload completed, the code performed a complicated series of file operations: reopening the temporary file in read mode to check length, closing it, renaming the existing destination file to a backup, renaming the temporary file to the destination, and finally deleting the backup. On an SPI-connected SD card, every directory modification involves multiple cluster operations; if any step hit a timing glitch, the entire upload was marked as failed.
  * **The Solution:** Simplify the finalization step: ensure the write buffer is flushed, close the temporary file, remove any existing file with the same target name, and execute a single rename from the temporary file to the final destination.

* **Network and Watchdog Starvation During Large Transfers**
  * **The Problem:** During multi-megabyte transfers, the processor spent 15 to 30 continuous seconds processing data inside the upload loop without yielding. This starved the FreeRTOS background tasks, delayed TCP acknowledgment packets, and risked triggering the hardware task watchdog timer.
  * **The Solution:** Insert a tiny 1-millisecond task delay between write bursts to allow the WiFi driver, network acknowledgment packets, and system watchdog timers to run smoothly.

---

### 2. Audio & Sensor Playback Behavior During Uploads

* **Pausing Music During File Transfers**
  * **The Problem:** Currently, if music is playing when an upload begins, it continues to play. The audio decoding task continually tries to read MP3 frames from the SD card over the SPI bus at the exact same time the web server is trying to write incoming data, causing bus collisions. Additionally, if the uploaded file happened to be the one currently playing, the firmware aborted the upload immediately.
  * **The Solution:**
    * When an upload starts, inspect the audio state and record a flag indicating whether playback was active.
    * Automatically pause playback so the audio engine releases its grip on the SD card and SPI bus.
    * Once the upload completes (whether successful or failed), check the saved flag and automatically resume playback from where it stopped.
    * Account for the edge case where the uploaded file overwrote the currently paused track by stopping playback or reloading the new file fresh rather than resuming a corrupted file handle.

* **RFID Reader Interference**
  * **The Problem:** The RFID card scanner continues polling every 120 ms even while an upload is in progress. If a disc is placed or bumped, it triggers a disc-mapping search on the SD card and attempts to start playing audio mid-upload.
  * **The Solution:** Temporarily disable the RFID polling engine for the duration of the upload, and re-enable it only after the upload finishes or aborts.

---

### 3. File Directory & Pagination Improvements

* **Crash-Prone Recursive Directory Scanning**
  * **The Problem:** The audio track listing endpoint recursively walked every single folder and subfolder on the SD card in a single operation. For cards with many files, recursively opening directory handles consumes critical stack memory (risking a crash) and produces a massive, unbroken JSON string that exhausts RAM.
  * **The Solution:** Eliminate deep recursive folder crawling in favor of single-level directory listing with strict boundaries.

* **Lack of Directory Pagination**
  * **The Problem:** When requesting a folder's contents, the server attempted to return all files at once. There was no way to request a subset of items, making it impossible for a user interface to display the first batch of files and load more on demand.
  * **The Solution:**
    * Update the directory browsing endpoint to accept pagination parameters (an offset to skip previously seen items, and a limit for the maximum number of items to return, such as 30).
    * Return metadata along with the file list, including the total number of items in the folder, the current offset, the limit, and a flag indicating whether more files exist to be loaded.

---

### 4. System Stability & Code Architecture

* **FreeRTOS Core Affinity & Priority Inversion**
  * **The Problem:** The audio task was configured at a high priority (Priority 5) on Core 1, while the main loop (which handles the web server, uploads, and user controls) ran on the same core at Priority 1. Because the audio task had a much higher priority, it constantly preempted the web server, causing network packet drops.
  * **The Solution:** Pin the audio decoding task to Core 0 (the network/protocol core), leaving Core 1 entirely dedicated to the main application loop, web server operations, and control handling.

* **Heap Fragmentation from Dynamic String Allocations**
  * **The Problem:** Throughout the disc mapping, CSV parsing, and server response handlers, dynamic string objects are repeatedly constructed, expanded, and destroyed on the heap. On an ESP32 without external PSRAM, this quickly fragments the limited internal RAM, eventually leading to random allocation failures.
  * **The Solution:** Replace frequent dynamic string operations with fixed-size, stack-allocated character arrays and bounded buffer reads, ensuring memory usage remains predictable.

* **Fragile Header-Only Static Variable Declarations**
  * **The Problem:** Core state variables and engine pointers were defined as static variables directly inside header files. In C++, this means that if any header is included across multiple source files, each file receives its own independent copy of the variable, leading to hidden state bugs and desynchronization.
  * **The Solution:** Move shared state variables into dedicated source files and expose them across headers using standard external references.
