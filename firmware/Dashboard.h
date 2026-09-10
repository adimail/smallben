#pragma once
#include <pgmspace.h>

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smallben</title>
<style>
:root{
  --bg:#121110;
  --panel:#191715;
  --panel-hover:#201d1a;
  --line:#262320;
  --text:#ede7df;
  --muted:#7d756d;
  --dim:#4e4943;
  --amber:#d99b43;
  --green:#52b788;
  --red:#d95d5d;
  --font-serif:Georgia,'Times New Roman',serif;
  --font-mono:ui-monospace,SFMono-Regular,Menlo,Consolas,'Liberation Mono',monospace;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:var(--font-mono);font-size:13px;line-height:1.6;-webkit-font-smoothing:antialiased}
.wrap{max-width:680px;margin:0 auto;padding:0 20px 80px}
.top{display:flex;justify-content:space-between;align-items:center;padding:24px 0 20px;border-bottom:1px solid var(--line);flex-wrap:wrap}
.brand{font-family:var(--font-serif);font-weight:600;font-size:22px;letter-spacing:-0.01em}
.status{display:flex;align-items:center;gap:8px;font-size:11px;color:var(--muted);letter-spacing:0.04em;flex-wrap:wrap}
.sys-info{font-size:11px;color:var(--dim);margin-right:4px}
.reader-bar{display:flex;align-items:center;gap:14px;padding:16px 20px;background:var(--panel);border:1px solid var(--line);border-radius:3px;margin:20px 0}
.disc-indicator{width:28px;height:28px;border-radius:50%;border:1px solid var(--line);background:var(--bg);display:flex;align-items:center;justify-content:center;flex-shrink:0}
.disc-dot{width:6px;height:6px;border-radius:50%;background:var(--amber);transition:background .2s}
.reader-text{min-width:0;flex-grow:1}
.reader-sub{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:0.05em}
.reader-headline{font-family:var(--font-serif);font-size:15px;color:var(--text);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.reader-controls{display:flex;align-items:center;gap:6px}
.btn-reader-action{background:none;border:none;color:var(--text);font-size:13px;cursor:pointer;padding:6px 8px;line-height:1;display:none;align-items:center;justify-content:center;transition:color .15s}
.btn-reader-action:hover{color:var(--amber)}
.btn-refresh{background:none;border:none;color:var(--muted);font-size:18px;cursor:pointer;padding:4px 6px;transition:color .15s,transform .25s;line-height:1;display:flex;align-items:center;justify-content:center}
.btn-refresh:hover{color:var(--amber)}
.btn-refresh.spin{transform:rotate(180deg)}

.nav-tabs{display:flex;border-bottom:1px solid var(--line);margin-bottom:24px;gap:8px;overflow-x:auto;overflow-y:hidden;white-space:nowrap;-webkit-overflow-scrolling:touch}
.nav-tabs::-webkit-scrollbar{display:none}
.nav-tab{background:none;border:none;color:var(--muted);font-family:var(--font-mono);font-size:13px;padding:10px 16px;cursor:pointer;border-bottom:2px solid transparent;transition:color .15s,border-color .15s;flex-shrink:0}
.nav-tab:hover{color:var(--text)}
.nav-tab.active{color:var(--amber);border-bottom-color:var(--amber)}

.view{display:none}
.view.active{display:block}

.head{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:16px}
.title{font-family:var(--font-serif);font-size:20px;font-weight:400}
.count{color:var(--dim);font-size:12px}

.track-row{display:grid;grid-template-columns:30px 1fr auto auto;gap:10px;padding:12px 0;border-bottom:1px solid var(--line);align-items:center;cursor:pointer;transition:background .15s}
.track-row:hover{background:rgba(255,255,255,0.02)}
.num{color:var(--dim);font-size:12px}
.track-main{min-width:0}
.track-title{color:var(--text);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.track-sub{color:var(--dim);font-size:11px;margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.track-size{color:var(--muted);font-size:12px;font-variant-numeric:tabular-nums}
.track-actions{display:flex;gap:6px}

.btn-icon{background:none;border:none;color:var(--dim);cursor:pointer;padding:6px 8px;font-family:inherit;font-size:12px;transition:color .15s}
.btn-icon:hover{color:var(--amber)}
.btn-del:hover{color:var(--red)}

.btn-loadmore{width:100%;padding:12px;background:var(--panel);border:1px solid var(--line);color:var(--amber);font-family:inherit;font-size:12px;cursor:pointer;text-align:center;transition:background .15s,border-color .15s;margin-top:16px;border-radius:2px}
.btn-loadmore:hover{background:var(--panel-hover);border-color:var(--dim)}

.plist-card{display:flex;justify-content:space-between;align-items:center;padding:18px 20px;background:var(--panel);border:1px solid var(--line);border-radius:2px;margin-bottom:10px;cursor:pointer;transition:background .15s,border-color .15s}
.plist-card:hover{background:var(--panel-hover);border-color:var(--dim)}
.plist-title{color:var(--text);font-size:14px;font-weight:400}
.plist-meta{color:var(--muted);font-size:12px;font-variant-numeric:tabular-nums;display:flex;gap:12px;align-items:center}

.btn-build{width:100%;padding:16px;background:transparent;border:1px dashed var(--line);color:var(--muted);font-family:inherit;font-size:12px;cursor:pointer;text-align:center;transition:border-color .15s,color .15s;margin-top:12px;border-radius:2px}
.btn-build:hover{border-color:var(--dim);color:var(--text)}

.bar-wrap{display:flex;justify-content:space-between;align-items:center;margin-bottom:16px}
.btn-link{background:none;border:none;color:var(--amber);cursor:pointer;font-family:inherit;font-size:12px;padding:0}
.btn-link:hover{text-decoration:underline}

.tree{background:var(--panel);border:1px solid var(--line);padding:14px 18px;border-radius:2px}
.tree ul{list-style:none;padding-left:18px}
.tree>ul{padding-left:0}
.tree li{padding:5px 0}
.tree-row{display:flex;justify-content:space-between;align-items:center;gap:8px}
.tree-left{display:flex;align-items:center;gap:8px;min-width:0}
.folder{color:var(--amber);cursor:pointer;user-select:none;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.file-dl{color:var(--text);text-decoration:none;cursor:pointer;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.file-dl:hover{color:var(--amber);text-decoration:underline}

.progress-box{display:none;width:100%;background:var(--panel);border:1px solid var(--line);height:14px;margin-bottom:16px;position:relative}
.progress-bar{height:100%;width:0%;background:var(--amber);transition:width .1s}
.progress-txt{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:9px;color:var(--text);font-weight:bold}
.msg{color:var(--dim);padding:20px 0;text-align:center}

.drawer-overlay{position:fixed;inset:0;background:rgba(0,0,0,0.6);opacity:0;pointer-events:none;transition:opacity .2s ease;z-index:90}
.drawer-overlay.open{opacity:1;pointer-events:auto}
.drawer{position:fixed;top:0;right:0;bottom:0;width:360px;max-width:88vw;background:var(--panel);border-left:1px solid var(--line);box-shadow:-6px 0 28px rgba(0,0,0,0.6);transform:translateX(100%);transition:transform .25s cubic-bezier(0.16,1,0.3,1);z-index:100;display:flex;flex-direction:column;padding:24px;overflow-y:auto}
.drawer.open{transform:translateX(0)}
.drawer-top{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px}
.drawer-heading{font-family:var(--font-serif);font-size:18px;font-weight:600}
.drawer-close{background:none;border:none;color:var(--muted);font-size:16px;cursor:pointer;padding:4px}
.drawer-close:hover{color:var(--text)}
.drawer-name{font-family:var(--font-serif);font-size:16px;color:var(--text);margin-bottom:16px;word-break:break-word}
.drawer-meta-list{display:flex;flex-direction:column;gap:14px;margin-bottom:24px;border-top:1px solid var(--line);border-bottom:1px solid var(--line);padding:16px 0}
.drawer-meta-item{display:flex;flex-direction:column;gap:4px}
.drawer-meta-label{font-size:10px;text-transform:uppercase;color:var(--muted);letter-spacing:0.05em}
.drawer-meta-val{font-size:12px;color:var(--text);word-break:break-all}
.drawer-tags{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.drawer-tag{background:var(--bg);border:1px solid var(--line);color:var(--amber);padding:3px 8px;border-radius:2px;font-size:11px}
.drawer-btn-group{display:flex;flex-direction:column;gap:10px}
.btn-play{display:block;width:100%;text-align:center;padding:12px;background:var(--green);color:#121110;border:none;cursor:pointer;font-family:inherit;font-weight:bold;font-size:12px;border-radius:2px;transition:opacity .15s}
.btn-play:hover{opacity:0.9}
.btn-download{display:block;width:100%;text-align:center;padding:12px;background:var(--amber);color:#121110;text-decoration:none;font-weight:bold;font-size:12px;border-radius:2px;transition:opacity .15s}
.btn-download:hover{opacity:0.9}

.disc-row{display:grid;grid-template-columns:1fr auto;gap:12px;padding:14px 16px;background:var(--panel);border:1px solid var(--line);border-radius:2px;margin-bottom:10px;align-items:center}
.disc-row.present{border-color:var(--amber)}
.disc-main{min-width:0}
.disc-title{font-size:14px;color:var(--text);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.disc-sub{font-size:11px;color:var(--muted);margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.badge-type{display:inline-block;padding:2px 6px;font-size:9px;text-transform:uppercase;border-radius:2px;background:var(--bg);border:1px solid var(--line);color:var(--amber);margin-right:6px}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <div class="brand">Smallben</div>
    <div class="status">
      <span class="sys-info" id="sysInfo"></span>
    </div>
  </div>

  <div class="reader-bar">
    <div class="disc-indicator"><div class="disc-dot" id="discDot"></div></div>
    <div class="reader-text">
      <div class="reader-sub" id="readerSub">reader status</div>
      <div class="reader-headline" id="readerHeadline">Waiting for disc...</div>
    </div>
    <div class="reader-controls">
      <button class="btn-icon" id="btnAssignPrompt" style="display:none;background:var(--amber);color:#121110;font-weight:bold;border-radius:2px;padding:6px 10px" onclick="openMapModal(activeTagUid)">Assign</button>
      <button class="btn-reader-action" id="btnReaderPlay" onclick="toggleCurrentPlay()">&#9654;</button>
      <button class="btn-refresh" id="btnRefresh" onclick="fetchNow()" title="Refresh Status">&#x21bb;</button>
    </div>
  </div>

  <div class="nav-tabs">
    <button class="nav-tab active" id="tabTracks" onclick="switchView('tracks')">Tracks</button>
    <button class="nav-tab" id="tabDiscs" onclick="switchView('discs')">Discs</button>
    <button class="nav-tab" id="tabPlaylists" onclick="switchView('playlists')">Playlists</button>
    <button class="nav-tab" id="tabFiles" onclick="switchView('files')">Files</button>
  </div>

  <div id="viewTracks" class="view active">
    <div class="head">
      <div class="title">Library</div>
      <div class="count" id="libCount">Scanning...</div>
    </div>
    <div id="trackList"><div class="msg">Reading library...</div></div>
    <button class="btn-loadmore" id="btnLoadMore" style="display:none"></button>
  </div>

  <div id="viewDiscs" class="view">
    <div class="head">
      <div class="title">Discs Registry</div>
      <div class="count" id="discsCount">...</div>
    </div>
    <div id="discsList"><div class="msg">Reading discs...</div></div>
  </div>

  <div id="viewPlaylists" class="view">
    <div id="plistMain">
      <div class="head">
        <div class="title">Playlists</div>
        <div class="count" id="plistCount">...</div>
      </div>
      <div id="plistCards"></div>
      <button class="btn-build" id="btnNewList">+ build a playlist from your library</button>
    </div>
    <div id="plistDetail" style="display:none">
      <div class="bar-wrap">
        <button class="btn-link" id="btnBackPlaylists">&larr; Back to all playlists</button>
        <div>
          <button class="btn-icon" id="btnRenameCurrent">Rename</button>
          <button class="btn-icon btn-del" id="btnDeleteCurrent">Delete</button>
        </div>
      </div>
      <div class="head">
        <div class="title" id="plistHeadTitle">Playlist</div>
        <div class="count" id="plistDetailCount">0 tracks</div>
      </div>
      <div id="detailTrackList"></div>
    </div>
  </div>

  <div id="viewFiles" class="view">
    <div class="head">
      <div class="title">File Storage</div>
      <button class="btn-link" id="btnUp">&#8593; Upload File</button>
      <input type="file" id="fileInput" style="display:none">
    </div>
    <div class="progress-box" id="pBox">
      <div class="progress-bar" id="pBar"></div>
      <div class="progress-txt" id="pTxt">0%</div>
    </div>
    <div class="tree" id="treeList">Loading tree...</div>
  </div>
</div>

<div class="drawer-overlay" id="mapModalOverlay" onclick="closeMapModal()"></div>
<div class="drawer" id="mapModal">
  <div class="drawer-top">
    <div class="drawer-heading">Map RFID Disc</div>
    <button class="drawer-close" onclick="closeMapModal()">&#10005;</button>
  </div>
  <div class="drawer-meta-list">
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Disc UID</span>
      <span class="drawer-meta-val" id="modalUidVal" style="font-weight:bold;color:var(--amber)"></span>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Label (Optional)</span>
      <input type="text" id="modalLabelInput" style="background:var(--bg);border:1px solid var(--line);color:var(--text);padding:8px;font-family:inherit;font-size:12px;border-radius:2px" placeholder="e.g. Bedtime Album">
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Playback Target Type</span>
      <select id="modalTypeSelect" onchange="onModalTypeChange()" style="background:var(--bg);border:1px solid var(--line);color:var(--text);padding:8px;font-family:inherit;font-size:12px;border-radius:2px">
        <option value="track">Single Track</option>
        <option value="playlist">Playlist</option>
      </select>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label" id="modalTargetLabel">Select Track</span>
      <select id="modalTargetSelect" style="background:var(--bg);border:1px solid var(--line);color:var(--text);padding:8px;font-family:inherit;font-size:12px;border-radius:2px"></select>
    </div>
  </div>
  <button class="btn-play" onclick="submitDiscMapping()">Save Mapping</button>
</div>

<div class="drawer-overlay" id="drawerOverlay" onclick="closeDrawer()"></div>
<div class="drawer" id="trackDrawer">
  <div class="drawer-top">
    <div class="drawer-heading">Track Details</div>
    <button class="drawer-close" onclick="closeDrawer()">&#10005;</button>
  </div>
  <div class="drawer-name" id="dName"></div>
  <div class="drawer-meta-list">
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Playlists</span>
      <div class="drawer-meta-val" id="dPlaylists">Scanning...</div>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Location</span>
      <span class="drawer-meta-val" id="dPath"></span>
    </div>
    <div class="drawer-meta-item">
      <span class="drawer-meta-label">Size</span>
      <span class="drawer-meta-val" id="dSize"></span>
    </div>
  </div>
  <div class="drawer-btn-group">
    <button class="btn-play" id="dPlayBtn">Play Track</button>
    <a class="btn-download" id="dDownload" href="#" download>Download Track</a>
  </div>
</div>

<script>
let tracks = [];
let playlists = [];
let discs = [];
let showAllTracks = false;
let currentPlaylist = null;
let currentPlaylistTracks = [];
let playingPath = '';
let playerState = 'stopped';
let activeTagUid = '';
let activeTagMapped = false;

const tabTracks = document.getElementById('tabTracks');
const tabDiscs = document.getElementById('tabDiscs');
const tabPlaylists = document.getElementById('tabPlaylists');
const tabFiles = document.getElementById('tabFiles');
const viewTracks = document.getElementById('viewTracks');
const viewDiscs = document.getElementById('viewDiscs');
const viewPlaylists = document.getElementById('viewPlaylists');
const viewFiles = document.getElementById('viewFiles');

const libCount = document.getElementById('libCount');
const trackList = document.getElementById('trackList');
const btnLoadMore = document.getElementById('btnLoadMore');

const discsCount = document.getElementById('discsCount');
const discsList = document.getElementById('discsList');

const plistCount = document.getElementById('plistCount');
const plistCards = document.getElementById('plistCards');
const plistMain = document.getElementById('plistMain');
const plistDetail = document.getElementById('plistDetail');
const plistHeadTitle = document.getElementById('plistHeadTitle');
const plistDetailCount = document.getElementById('plistDetailCount');
const detailTrackList = document.getElementById('detailTrackList');
const btnNewList = document.getElementById('btnNewList');
const btnBackPlaylists = document.getElementById('btnBackPlaylists');
const btnRenameCurrent = document.getElementById('btnRenameCurrent');
const btnDeleteCurrent = document.getElementById('btnDeleteCurrent');

const treeList = document.getElementById('treeList');
const btnUp = document.getElementById('btnUp');
const fileInput = document.getElementById('fileInput');
const pBox = document.getElementById('pBox');
const pBar = document.getElementById('pBar');
const pTxt = document.getElementById('pTxt');

const drawerOverlay = document.getElementById('drawerOverlay');
const trackDrawer = document.getElementById('trackDrawer');
const dName = document.getElementById('dName');
const dPlaylists = document.getElementById('dPlaylists');
const dPath = document.getElementById('dPath');
const dSize = document.getElementById('dSize');
const dDownload = document.getElementById('dDownload');
const dPlayBtn = document.getElementById('dPlayBtn');
const readerHeadline = document.getElementById('readerHeadline');
const readerSub = document.getElementById('readerSub');
const discDot = document.getElementById('discDot');
const btnRefresh = document.getElementById('btnRefresh');
const btnReaderPlay = document.getElementById('btnReaderPlay');
const btnAssignPrompt = document.getElementById('btnAssignPrompt');
const sysInfo = document.getElementById('sysInfo');

const mapModalOverlay = document.getElementById('mapModalOverlay');
const mapModal = document.getElementById('mapModal');
const modalUidVal = document.getElementById('modalUidVal');
const modalLabelInput = document.getElementById('modalLabelInput');
const modalTypeSelect = document.getElementById('modalTypeSelect');
const modalTargetLabel = document.getElementById('modalTargetLabel');
const modalTargetSelect = document.getElementById('modalTargetSelect');

function fmt(b){
  if(b < 1048576) return (b/1024).toFixed(1) + ' KB';
  if(b < 1073741824) return (b/1048576).toFixed(1) + ' MB';
  return (b/1073741824).toFixed(1) + ' GB';
}

function cleanName(n){ return n.replace(/\.[^/.]+$/, ''); }

function fmtUptime(s){
  if(s < 60) return s + 's';
  const m = Math.floor(s / 60);
  const remS = s % 60;
  if(m < 60) return m + 'm ' + remS + 's';
  const h = Math.floor(m / 60);
  const remM = m % 60;
  return h + 'h ' + remM + 'm';
}

function switchView(viewName){
  tabTracks.classList.toggle('active', viewName === 'tracks');
  tabDiscs.classList.toggle('active', viewName === 'discs');
  tabPlaylists.classList.toggle('active', viewName === 'playlists');
  tabFiles.classList.toggle('active', viewName === 'files');
  viewTracks.classList.toggle('active', viewName === 'tracks');
  viewDiscs.classList.toggle('active', viewName === 'discs');
  viewPlaylists.classList.toggle('active', viewName === 'playlists');
  viewFiles.classList.toggle('active', viewName === 'files');
  if(viewName === 'files') loadTree();
  if(viewName === 'discs') loadDiscs();
  if(viewName === 'playlists' && !currentPlaylist) loadPlaylists();
  if(viewName === 'tracks' && !tracks.length) loadLibrary();
}

function fetchNow(){
  if(btnRefresh) btnRefresh.classList.add('spin');
  fetch('/now')
    .then(r => r.json())
    .then(d => {
      if(btnRefresh) setTimeout(() => btnRefresh.classList.remove('spin'), 250);
      playerState = d.player.state;
      playingPath = d.player.path;

      if(sysInfo){
        const parts = [];
        if(d.system && typeof d.system.freeHeap === 'number'){
          parts.push(fmt(d.system.freeHeap) + ' RAM');
        }
        if(d.sd && d.sd.ready && typeof d.sd.totalBytes === 'number' && typeof d.sd.usedBytes === 'number'){
          const freeBytes = d.sd.totalBytes > d.sd.usedBytes ? (d.sd.totalBytes - d.sd.usedBytes) : 0;
          parts.push(fmt(freeBytes) + ' SD free');
        } else if(d.sd && !d.sd.ready){
          parts.push('No SD');
        }
        if(d.system && typeof d.system.uptime === 'number'){
          parts.push('up ' + fmtUptime(d.system.uptime));
        }
        sysInfo.textContent = parts.join(' \u00B7 ');
      }

      if(d.rfid && d.rfid.present){
        activeTagUid = d.rfid.uid;
        activeTagMapped = d.rfid.mapped;
      } else {
        activeTagUid = '';
        activeTagMapped = false;
      }

      if(!d.sd.ready){
        discDot.style.background = 'var(--red)';
        readerHeadline.textContent = 'SD Card Error / Not Detected';
        readerSub.textContent = 'storage error';
        btnReaderPlay.style.display = 'none';
        btnAssignPrompt.style.display = 'none';
      } else if(d.rfid && d.rfid.present && !d.rfid.mapped){
        discDot.style.background = 'var(--amber)';
        readerHeadline.textContent = 'Unmapped Tag: ' + d.rfid.uid;
        readerSub.textContent = 'unmapped disc detected';
        btnReaderPlay.style.display = 'none';
        btnAssignPrompt.style.display = 'flex';
      } else if(playerState === 'playing'){
        btnAssignPrompt.style.display = 'none';
        discDot.style.background = 'var(--green)';
        const title = d.rfid && d.rfid.present && d.rfid.label ? d.rfid.label : cleanName(d.player.name || playingPath.split('/').pop());
        readerHeadline.textContent = 'Playing: ' + title;
        readerSub.textContent = d.rfid && d.rfid.present ? 'disc playing (' + d.rfid.uid + ')' : 'reader active';
        btnReaderPlay.style.display = 'flex';
        btnReaderPlay.innerHTML = '&#10074;&#10074;';
      } else if(playerState === 'paused'){
        btnAssignPrompt.style.display = 'none';
        discDot.style.background = 'var(--amber)';
        const title = d.rfid && d.rfid.present && d.rfid.label ? d.rfid.label : cleanName(d.player.name || playingPath.split('/').pop());
        readerHeadline.textContent = 'Paused: ' + title;
        readerSub.textContent = 'reader paused';
        btnReaderPlay.style.display = 'flex';
        btnReaderPlay.innerHTML = '&#9654;';
      } else {
        btnAssignPrompt.style.display = 'none';
        discDot.style.background = 'var(--amber)';
        readerHeadline.textContent = 'Waiting for disc...';
        readerSub.textContent = 'reader ready';
        btnReaderPlay.style.display = 'none';
      }
      renderTracksSlice();
      if(currentPlaylist) renderPlaylistTracks();
      if(viewDiscs.classList.contains('active')) renderDiscs();
    })
    .catch(() => {
      if(btnRefresh) btnRefresh.classList.remove('spin');
      discDot.style.background = 'var(--red)';
      readerHeadline.textContent = 'Device Unreachable';
      readerSub.textContent = 'offline';
      btnReaderPlay.style.display = 'none';
      btnAssignPrompt.style.display = 'none';
    });
}

function toggleCurrentPlay(){
  if(!playingPath) return;
  toggleTrackPlay(playingPath);
}

function toggleTrackPlay(path){
  fetch('/api/player/toggle?path=' + encodeURIComponent(path), { method: 'POST' })
    .then(r => r.json())
    .then(d => {
      if(d.status === 'ok'){
        fetchNow();
      }
    }).catch(()=>{});
}

function renderTracksSlice(){
  if(!tracks.length){
    trackList.innerHTML = '<div class="msg">No tracks found on storage.</div>';
    btnLoadMore.style.display = 'none';
    return;
  }
  const visible = showAllTracks ? tracks : tracks.slice(0, 5);
  trackList.innerHTML = visible.map((t, i) => {
    const isThisPlaying = (playingPath === t.path && playerState === 'playing');
    const playIcon = isThisPlaying ? '&#10074;&#10074;' : '&#9654;';
    return `
      <div class="track-row" onclick="openDrawer(${i})">
        <span class="num">${String(i+1).padStart(2,'0')}</span>
        <div class="track-main">
          <div class="track-title">${cleanName(t.name)}</div>
          <div class="track-sub">${t.path}</div>
        </div>
        <span class="track-size">${fmt(t.size)}</span>
        <div class="track-actions">
          <button class="btn-icon" onclick="event.stopPropagation();toggleTrackPlay('${t.path.replace(/'/g,"\\'")}')">${playIcon}</button>
          <button class="btn-icon" onclick="event.stopPropagation();addToPlaylistPrompt('${t.path.replace(/'/g,"\\'")}')">+ list</button>
        </div>
      </div>
    `;
  }).join('');

  if(tracks.length > 5){
    btnLoadMore.style.display = 'block';
    btnLoadMore.textContent = showAllTracks ? 'Show Less' : `Show All Tracks (${tracks.length - 5} more)`;
  } else {
    btnLoadMore.style.display = 'none';
  }
}

btnLoadMore.onclick = () => {
  showAllTracks = !showAllTracks;
  renderTracksSlice();
};

function closeDrawer(){
  drawerOverlay.classList.remove('open');
  trackDrawer.classList.remove('open');
}

async function openDrawer(idx){
  const t = tracks[idx];
  if(!t) return;
  dName.textContent = t.name;
  dPath.textContent = t.path;
  dSize.textContent = fmt(t.size);
  dPlaylists.textContent = 'Scanning playlists...';
  dDownload.href = '/api/download?path=' + encodeURIComponent(t.path);
  dDownload.setAttribute('download', t.name);

  dPlayBtn.onclick = () => {
    toggleTrackPlay(t.path);
  };

  drawerOverlay.classList.add('open');
  trackDrawer.classList.add('open');

  try{
    const res = await fetch('/api/track/playlists?path=' + encodeURIComponent(t.path));
    const matchNames = await res.json();
    if(matchNames.length === 0){
      dPlaylists.innerHTML = '<span>Part of 0 playlists</span>';
    } else {
      const countLabel = matchNames.length === 1 ? 'Part of 1 playlist:' : `Part of ${matchNames.length} playlists:`;
      const tags = matchNames.map(name => `<span class="drawer-tag">${name}</span>`).join('');
      dPlaylists.innerHTML = `<div>${countLabel}</div><div class="drawer-tags">${tags}</div>`;
    }
  }catch(e){
    dPlaylists.textContent = 'Unavailable';
  }
}

function loadLibrary(){
  fetch('/api/mp3').then(r=>r.json()).then(d=>{
    tracks = d;
    libCount.textContent = d.length + ' tracks on sd card';
    renderTracksSlice();
  }).catch(()=>{ trackList.innerHTML = '<div class="msg">Error reading storage.</div>'; });
}

function loadPlaylists(){
  fetch('/api/playlists').then(r=>r.json()).then(d=>{
    playlists = d;
    plistCount.textContent = d.length + ' playlists';
    if(!d.length){
      plistCards.innerHTML = '<div class="msg">No playlists created yet.</div>';
      return;
    }
    plistCards.innerHTML = d.map(p => {
      const base = p.name.replace('.m3u','');
      return `
        <div class="plist-card" onclick="openPlaylist('${base}')">
          <span class="plist-title">${base}</span>
          <div class="plist-meta">
            <span>${p.trackCount} tracks</span>
          </div>
        </div>
      `;
    }).join('');
  }).catch(()=>{ plistCards.innerHTML = '<div class="msg">Failed to load playlists.</div>'; });
}

function loadDiscs(){
  fetch('/api/discs').then(r=>r.json()).then(d=>{
    discs = d;
    discsCount.textContent = d.length + ' registered discs';
    renderDiscs();
  }).catch(()=>{ discsList.innerHTML = '<div class="msg">Failed to load discs registry.</div>'; });
}

function renderDiscs(){
  let banner = '';
  if(activeTagUid && !activeTagMapped){
    banner = `
      <div style="margin-bottom:16px;padding:14px 16px;background:var(--panel);border:1px solid var(--amber);border-radius:2px;display:flex;justify-content:space-between;align-items:center">
        <div>
          <div style="font-size:10px;color:var(--amber);text-transform:uppercase;letter-spacing:0.05em">Active Unmapped Tag</div>
          <div style="font-size:14px;color:var(--text);font-weight:bold">${activeTagUid}</div>
        </div>
        <button class="btn-icon" style="background:var(--amber);color:#121110;font-weight:bold;padding:6px 12px;border-radius:2px" onclick="openMapModal('${activeTagUid}')">Map Disc</button>
      </div>
    `;
  }
  if(!discs.length && !activeTagUid){
    discsList.innerHTML = banner + '<div class="msg">No discs mapped yet. Place an NFC tag near the reader to map it.</div>';
    return;
  }
  const rows = discs.map(d => {
    const isPresent = (activeTagUid === d.uid);
    return `
      <div class="disc-row ${isPresent ? 'present' : ''}">
        <div class="disc-main">
          <div class="disc-title">${d.label || d.target}</div>
          <div class="disc-sub">
            <span class="badge-type">${d.type}</span>
            <span style="color:var(--dim);margin-right:8px">${d.uid}</span>
            <span>${d.target}</span>
          </div>
        </div>
        <div class="track-actions">
          <button class="btn-icon" onclick="openMapModal('${d.uid}', '${d.type}', '${d.target.replace(/'/g,"\\'")}', '${(d.label||'').replace(/'/g,"\\'")}')">Edit</button>
          <button class="btn-icon btn-del" onclick="deleteDiscMapping('${d.uid}')">&#10005;</button>
        </div>
      </div>
    `;
  }).join('');
  discsList.innerHTML = banner + rows;
}

function deleteDiscMapping(uid){
  if(!confirm('Remove mapping for disc ' + uid + '?')) return;
  fetch('/api/discs?uid=' + encodeURIComponent(uid), { method: 'DELETE' })
    .then(r=>r.json())
    .then(d=>{
      if(d.status === 'ok'){
        loadDiscs();
        fetchNow();
      } else alert(d.error || 'Failed to remove disc');
    }).catch(()=>alert('Network error'));
}

function openMapModal(uid, type = 'track', target = '', label = ''){
  if(!tracks.length) loadLibrary();
  if(!playlists.length) loadPlaylists();

  modalUidVal.textContent = uid;
  modalLabelInput.value = label;
  modalTypeSelect.value = type;

  populateModalTargets(type, target);

  mapModalOverlay.classList.add('open');
  mapModal.classList.add('open');
}

function closeMapModal(){
  mapModalOverlay.classList.remove('open');
  mapModal.classList.remove('open');
}

function onModalTypeChange(){
  populateModalTargets(modalTypeSelect.value);
}

function populateModalTargets(type, selected = ''){
  modalTargetSelect.innerHTML = '';
  if(type === 'track'){
    modalTargetLabel.textContent = 'Select Track';
    tracks.forEach(t => {
      const opt = document.createElement('option');
      opt.value = t.path;
      opt.textContent = t.name;
      if(t.path === selected) opt.selected = true;
      modalTargetSelect.appendChild(opt);
    });
  } else {
    modalTargetLabel.textContent = 'Select Playlist';
    playlists.forEach(p => {
      const base = p.name.replace('.m3u', '');
      const opt = document.createElement('option');
      opt.value = base;
      opt.textContent = base + ' (' + p.trackCount + ' tracks)';
      if(base === selected) opt.selected = true;
      modalTargetSelect.appendChild(opt);
    });
  }
}

function submitDiscMapping(){
  const uid = modalUidVal.textContent;
  const label = modalLabelInput.value.trim();
  const type = modalTypeSelect.value;
  const target = modalTargetSelect.value;

  if(!target){
    alert('Please select a target song or playlist.');
    return;
  }

  fetch('/api/discs/map', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ uid, type, target, label })
  }).then(r=>r.json()).then(d=>{
    if(d.status === 'ok'){
      closeMapModal();
      loadDiscs();
      fetchNow();
    } else {
      alert(d.error || 'Failed to save mapping');
    }
  }).catch(()=>alert('Network error'));
}

function openPlaylist(name){
  currentPlaylist = name;
  plistMain.style.display = 'none';
  plistDetail.style.display = 'block';
  plistHeadTitle.textContent = name;
  plistDetailCount.textContent = 'Loading...';
  detailTrackList.innerHTML = '<div class="msg">Loading tracks...</div>';
  fetch('/api/playlists/' + encodeURIComponent(name) + '/tracks')
    .then(r=>r.json())
    .then(list => {
      currentPlaylistTracks = list;
      renderPlaylistTracks();
    }).catch(()=>{ detailTrackList.innerHTML = '<div class="msg">Failed to load playlist tracks.</div>'; });
}

function renderPlaylistTracks(){
  plistDetailCount.textContent = currentPlaylistTracks.length + ' tracks';
  if(!currentPlaylistTracks.length){
    detailTrackList.innerHTML = '<div class="msg">This playlist is empty.</div>';
    return;
  }
  detailTrackList.innerHTML = currentPlaylistTracks.map((t, i) => {
    const isThisPlaying = (playingPath === t.path && playerState === 'playing');
    const playIcon = isThisPlaying ? '&#10074;&#10074;' : '&#9654;';
    return `
      <div class="track-row">
        <span class="num">${String(i+1).padStart(2,'0')}</span>
        <div class="track-main">
          <div class="track-title">${cleanName(t.name)}</div>
          <div class="track-sub">${t.path}</div>
        </div>
        <span class="track-size">${fmt(t.size)}</span>
        <div class="track-actions">
          <button class="btn-icon" onclick="toggleTrackPlay('${t.path.replace(/'/g,"\\'")}')">${playIcon}</button>
          <button class="btn-icon" onclick="moveTrack(${i}, -1)">&#9650;</button>
          <button class="btn-icon" onclick="moveTrack(${i}, 1)">&#9660;</button>
          <button class="btn-icon btn-del" onclick="removeTrack(${i})">&#10005;</button>
        </div>
      </div>
    `;
  }).join('');
}

function savePlaylistOrder(){
  const paths = currentPlaylistTracks.map(t => t.path);
  fetch('/api/playlists/tracks?id=' + encodeURIComponent(currentPlaylist), {
    method: 'PUT',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({tracks: paths})
  }).then(r=>r.json()).then(d=>{
    if(d.status !== 'ok') alert('Failed to sync playlist.');
  }).catch(()=>alert('Network error while saving playlist.'));
}

function moveTrack(idx, dir){
  const target = idx + dir;
  if(target < 0 || target >= currentPlaylistTracks.length) return;
  const temp = currentPlaylistTracks[idx];
  currentPlaylistTracks[idx] = currentPlaylistTracks[target];
  currentPlaylistTracks[target] = temp;
  renderPlaylistTracks();
  savePlaylistOrder();
}

function removeTrack(idx){
  currentPlaylistTracks.splice(idx, 1);
  renderPlaylistTracks();
  savePlaylistOrder();
}

btnBackPlaylists.onclick = () => {
  currentPlaylist = null;
  plistDetail.style.display = 'none';
  plistMain.style.display = 'block';
  loadPlaylists();
};

btnNewList.onclick = () => {
  const name = prompt('Name your playlist:');
  if(!name) return;
  fetch('/api/playlists', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: name.trim()})
  }).then(r=>r.json()).then(d=>{
    if(d.status === 'ok') loadPlaylists();
    else alert(d.error || 'Failed to create playlist');
  }).catch(()=>alert('Network error'));
};

btnRenameCurrent.onclick = () => {
  if(!currentPlaylist) return;
  const n = prompt('Rename playlist to:', currentPlaylist);
  if(!n || n === currentPlaylist) return;
  fetch('/api/playlists/' + encodeURIComponent(currentPlaylist), {
    method: 'PUT',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({newName: n.trim()})
  }).then(r=>r.json()).then(d=>{
    if(d.status === 'ok'){
      currentPlaylist = n.trim();
      plistHeadTitle.textContent = currentPlaylist;
    } else {
      alert(d.error || 'Failed to rename');
    }
  }).catch(()=>alert('Network error'));
};

btnDeleteCurrent.onclick = () => {
  if(!currentPlaylist) return;
  if(!confirm('Permanently delete playlist: ' + currentPlaylist + '?')) return;
  fetch('/api/playlists/' + encodeURIComponent(currentPlaylist), { method: 'DELETE' })
    .then(r=>r.json())
    .then(d=>{
      if(d.status === 'ok') btnBackPlaylists.click();
      else alert(d.error || 'Failed to delete');
    }).catch(()=>alert('Network error'));
};

function addToPlaylistPrompt(path){
  if(!playlists.length){
    fetch('/api/playlists').then(r=>r.json()).then(d=>{
      playlists = d;
      showAddPrompt(path);
    }).catch(()=>alert('No playlists found.'));
    return;
  }
  showAddPrompt(path);
}

function showAddPrompt(path){
  if(!playlists.length){
    alert('Please create a playlist first.');
    return;
  }
  const opts = playlists.map((p, i) => `${i+1}: ${p.name.replace('.m3u','')}`).join('\n');
  const sel = prompt('Select playlist number:\n' + opts);
  const idx = parseInt(sel, 10) - 1;
  if(isNaN(idx) || idx < 0 || idx >= playlists.length) return;
  const target = playlists[idx].name.replace('.m3u','');
  fetch('/api/playlists/tracks?id=' + encodeURIComponent(target), {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({path: path})
  }).then(r=>r.json()).then(d=>{
    if(d.status === 'ok'){
      loadPlaylists();
      alert('Track added to ' + target);
    } else {
      alert(d.error || 'Failed to add');
    }
  }).catch(()=>alert('Network error'));
}

function isDbOrSysFile(name){
  const n = name.toLowerCase();
  return n.endsWith('.db') || n.endsWith('.dat') || n.endsWith('.idx') || n.endsWith('.bin') || n.endsWith('.sys') || n === 'thumbs.db' || n === '.ds_store' || n === 'discs.csv';
}

function fetchLevel(path){
  return fetch('/api/tree?path=' + encodeURIComponent(path)).then(r=>r.json());
}

function createTreeItem(node, parentPath){
  if(node.type !== 'dir' && isDbOrSysFile(node.name)) return null;
  const li = document.createElement('li');
  const full = (parentPath === '/' ? '' : parentPath) + '/' + node.name;
  if(node.type === 'dir'){
    const row = document.createElement('div');
    row.className = 'tree-row';
    const left = document.createElement('div');
    left.className = 'tree-left';
    const label = document.createElement('span');
    label.className = 'folder';
    label.textContent = '\u25B8 ' + node.name + '/';
    left.appendChild(label);
    row.appendChild(left);
    const childUl = document.createElement('ul');
    childUl.style.display = 'none';
    let loaded = false, loading = false;
    label.onclick = () => {
      const open = childUl.style.display === 'block';
      if(open){
        childUl.style.display = 'none';
        label.textContent = '\u25B8 ' + node.name + '/';
        return;
      }
      label.textContent = '\u25BE ' + node.name + '/';
      childUl.style.display = 'block';
      if(loaded || loading) return;
      loading = true;
      childUl.innerHTML = '<li class="msg">Loading...</li>';
      fetchLevel(full).then(children => {
        loaded = true; loading = false;
        childUl.innerHTML = '';
        const valid = children.filter(c => c.type === 'dir' || !isDbOrSysFile(c.name));
        if(!valid.length){ childUl.innerHTML = '<li class="msg">Empty</li>'; return; }
        valid.forEach(c => {
          const el = createTreeItem(c, full);
          if(el) childUl.appendChild(el);
        });
      }).catch(()=>{ loading = false; childUl.innerHTML = '<li class="msg">Error</li>'; });
    };
    li.appendChild(row);
    li.appendChild(childUl);
  } else {
    li.innerHTML = `
      <div class="tree-row">
        <div class="tree-left">
          <a class="file-dl" href="/api/download?path=${encodeURIComponent(full)}" download="${node.name}">${node.name}</a>
          <span class="track-size" style="font-size:11px">${fmt(node.size)}</span>
        </div>
        <button class="btn-icon btn-del" onclick="confirmDelete('${full.replace(/'/g,"\\'")}')">&#10005;</button>
      </div>
    `;
  }
  return li;
}

function loadTree(){
  treeList.innerHTML = '<div class="msg">Loading filesystem...</div>';
  fetchLevel('/').then(nodes => {
    treeList.innerHTML = '';
    const valid = nodes.filter(n => n.type === 'dir' || !isDbOrSysFile(n.name));
    if(!valid.length){ treeList.innerHTML = '<div class="msg">Empty card.</div>'; return; }
    const ul = document.createElement('ul');
    valid.forEach(n => {
      const el = createTreeItem(n, '/');
      if(el) ul.appendChild(el);
    });
    treeList.appendChild(ul);
  }).catch(()=>{ treeList.innerHTML = '<div class="msg">Error reading files.</div>'; });
}

function confirmDelete(path){
  if(!confirm('Permanently delete ' + path + '?')) return;
  fetch('/api/delete?path=' + encodeURIComponent(path), { method: 'DELETE' })
    .then(r=>r.json()).then(d=>{
      if(d.status === 'ok'){
        loadTree();
        loadLibrary();
      } else alert(d.error || 'Failed to delete');
    }).catch(()=>alert('Network error'));
}

btnUp.onclick = () => fileInput.click();

fileInput.onchange = () => {
  if(!fileInput.files || fileInput.files.length === 0) return;
  const file = fileInput.files[0];
  if(file.size > 31457280){
    alert('File exceeds the 30MB size limit.');
    fileInput.value = '';
    return;
  }
  pBox.style.display = 'block';
  pBar.style.width = '0%';
  pTxt.textContent = '0%';
  btnUp.disabled = true;
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload?path=/');
  xhr.upload.onprogress = e => {
    if(e.lengthComputable){
      const pct = Math.round((e.loaded / e.total) * 100);
      pBar.style.width = pct + '%';
      pTxt.textContent = pct + '%';
    }
  };
  xhr.onload = () => {
    pBox.style.display = 'none';
    btnUp.disabled = false;
    fileInput.value = '';
    if(xhr.status >= 200 && xhr.status < 300){
      loadLibrary();
      loadTree();
    } else {
      alert('Upload failed: ' + (xhr.responseText || 'Server error'));
    }
  };
  xhr.onerror = () => {
    pBox.style.display = 'none';
    btnUp.disabled = false;
    fileInput.value = '';
    alert('Upload network error.');
  };
  const form = new FormData();
  form.append('file', file, file.name);
  xhr.send(form);
};

fetchNow();
setInterval(fetchNow, 2000);
loadLibrary();
loadPlaylists();
loadDiscs();
</script>
</body>
</html>
)HTML";
