#pragma once
#include <pgmspace.h>

const char DASHBOARD_SIDEBAR_HTML[] PROGMEM = R"HTML(
<div class="drawer-overlay" id="drawerOverlay"></div>

<div class="drawer" id="trackDrawer">
  <div class="drawer-top">
    <div class="drawer-heading">Track Actions</div>
    <button class="drawer-close" id="btnTrackDrawerClose">&#10005;</button>
  </div>
  <div class="drawer-name" id="dTrackName"></div>
  <div class="drawer-meta-list">
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Path</span>
      <span class="drawer-meta-val" id="dTrackPath"></span>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Size</span>
      <span class="drawer-meta-val" id="dTrackSize"></span>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Playlists</span>
      <div class="drawer-meta-val" id="dTrackPlaylists">Loading...</div>
    </div>
  </div>
  <div class="drawer-btn-group">
    <button class="btn-action btn-success" id="btnSidebarPlay">Play</button>
    <button class="btn-action btn-sec" id="btnSidebarAddPlist">+ Add to Playlist</button>
    <div id="sidebarAddPlistBox" class="inline-form" style="display:none;margin-top:4px">
      <select id="sidebarPlistSelect" class="input-text"></select>
      <div class="form-actions">
        <button class="btn-icon" id="btnCancelAddPlist">Cancel</button>
        <button class="btn-icon" id="btnConfirmAddPlist" style="color:var(--amber);font-weight:bold">Add</button>
      </div>
    </div>
    <button class="btn-action btn-primary" id="btnSidebarMapDisc">Map to Disc</button>
    <a class="btn-action btn-sec" id="btnSidebarDownload" href="#" download>Download Track</a>
    <button class="btn-action btn-danger" id="btnSidebarDelete">Delete Track</button>
  </div>
</div>

<div class="drawer" id="plistDrawer">
  <div class="drawer-top">
    <div class="drawer-heading">Playlist Actions</div>
    <button class="drawer-close" id="btnPlistDrawerClose">&#10005;</button>
  </div>
  <div class="drawer-name" id="dPlistName"></div>
  <div class="drawer-meta-list">
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Rename Playlist</span>
      <input type="text" id="inputRenamePlist" class="input-text">
    </div>
  </div>
  <div class="drawer-btn-group">
    <button class="btn-action btn-primary" id="btnSavePlistRename">Save New Name</button>
    <button class="btn-action btn-primary" id="btnMapPlistDisc">Map Playlist to Disc</button>
    <button class="btn-action btn-danger" id="btnDeletePlist">Delete Playlist</button>
  </div>
</div>

<div class="drawer" id="mapModal">
  <div class="drawer-top">
    <div class="drawer-heading">Map RFID Disc</div>
    <button class="drawer-close" id="btnMapModalClose">&#10005;</button>
  </div>
  <div class="drawer-meta-list">
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Disc UID</span>
      <span class="drawer-meta-val" id="modalUidVal" style="font-weight:bold;color:var(--amber)"></span>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Label (Optional)</span>
      <input type="text" id="modalLabelInput" class="input-text" placeholder="e.g. Bedtime Album">
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Playback Target Type</span>
      <select id="modalTypeSelect" class="input-text">
        <option value="track">Single Track</option>
        <option value="playlist">Playlist</option>
      </select>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label" id="modalTargetLabel">Select Target</span>
      <select id="modalTargetSelect" class="input-text"></select>
    </div>
  </div>
  <button class="btn-action btn-primary" id="btnModalSave">Save Mapping</button>
</div>
)HTML";
