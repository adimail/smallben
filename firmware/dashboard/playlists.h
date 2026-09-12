#pragma once
#include <pgmspace.h>

const char DASHBOARD_PLAYLISTS_HTML[] PROGMEM = R"HTML(
<div id="viewPlaylists" class="view">
  <div id="plistMain">
    <div class="head">
      <div class="title">Playlists</div>
      <div class="count" id="plistCount">...</div>
    </div>
    <div id="plistCards"></div>
    <button class="btn-build" id="btnNewList">+ build a playlist</button>
    <div id="newPlistForm" class="inline-form" style="display:none">
      <input type="text" id="newPlistNameInput" class="input-text" placeholder="Playlist name">
      <div class="form-actions">
        <button class="btn-icon" id="btnCancelNewPlist">Cancel</button>
        <button class="btn-icon" id="btnConfirmNewPlist" style="color:var(--amber);font-weight:bold">Create</button>
      </div>
    </div>
  </div>
  <div id="plistDetail" style="display:none">
    <div class="bar-wrap">
      <button class="btn-link" id="btnBackPlaylists">&larr; Back to all playlists</button>
      <button class="btn-icon" id="btnOpenPlistSettings" style="color:var(--amber)">Settings &#9881;</button>
    </div>
    <div class="head">
      <div class="title" id="plistHeadTitle">Playlist</div>
      <div class="count" id="plistDetailCount">0 tracks</div>
    </div>
    <div id="detailTrackList"></div>
  </div>
</div>
)HTML";
