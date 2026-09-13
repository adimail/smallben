#pragma once
#include <pgmspace.h>

const char DASHBOARD_TRACKS_HTML[] PROGMEM = R"HTML(
<div id="viewTracks" class="view active">
  <div class="head">
    <div class="title">Library</div>
    <div class="count" id="libCount">Scanning...</div>
  </div>
  <div id="trackList"><div class="msg">Reading library...</div></div>
  <button class="btn-loadmore" id="btnLoadMore" style="display:none"></button>
</div>
)HTML";
