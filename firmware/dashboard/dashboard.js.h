#pragma once
#include <pgmspace.h>

const char DASHBOARD_JS[] PROGMEM = R"JS(
let tracks = [];
let playlists = [];
let discs = [];
let currentPlaylist = null;
let currentPlaylistTracks = [];
let playingPath = '';
let playerState = 'stopped';
let activeTagUid = '';
let activeTagMapped = false;
let isFetchingNow = false;
let currentFolder = '/';
let activeSelectedTrack = null;
let toastTimer = null;

let filesOffset = 0;
let filesLimit = 30;
let filesHasMore = false;

let tracksOffset = 0;
let tracksLimit = 30;
let tracksHasMore = false;
let tracksTotal = 0;

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
const newPlistForm = document.getElementById('newPlistForm');
const newPlistNameInput = document.getElementById('newPlistNameInput');
const btnConfirmNewPlist = document.getElementById('btnConfirmNewPlist');
const btnCancelNewPlist = document.getElementById('btnCancelNewPlist');
const btnBackPlaylists = document.getElementById('btnBackPlaylists');
const btnOpenPlistSettings = document.getElementById('btnOpenPlistSettings');

const btnUp = document.getElementById('btnUp');
const fileInput = document.getElementById('fileInput');
const pBox = document.getElementById('pBox');
const pBar = document.getElementById('pBar');
const pTxt = document.getElementById('pTxt');
const fileBreadcrumbs = document.getElementById('fileBreadcrumbs');
const browserFileList = document.getElementById('browserFileList');
const btnFilesLoadMore = document.getElementById('btnFilesLoadMore');
const btnNewFolder = document.getElementById('btnNewFolder');
const newFolderForm = document.getElementById('newFolderForm');
const newFolderNameInput = document.getElementById('newFolderNameInput');
const btnConfirmNewFolder = document.getElementById('btnConfirmNewFolder');
const btnCancelNewFolder = document.getElementById('btnCancelNewFolder');

const readerHeadline = document.getElementById('readerHeadline');
const readerSub = document.getElementById('readerSub');
const discDot = document.getElementById('discDot');
const btnRefresh = document.getElementById('btnRefresh');
const btnReaderPlay = document.getElementById('btnReaderPlay');
const btnAssignPrompt = document.getElementById('btnAssignPrompt');
const sysInfo = document.getElementById('sysInfo');

const drawerOverlay = document.getElementById('drawerOverlay');
const trackDrawer = document.getElementById('trackDrawer');
const dTrackName = document.getElementById('dTrackName');
const dTrackPath = document.getElementById('dTrackPath');
const dTrackSize = document.getElementById('dTrackSize');
const dTrackPlaylists = document.getElementById('dTrackPlaylists');
const dHeroPlayState = document.getElementById('dHeroPlayState');
const btnHeroPlay = document.getElementById('btnHeroPlay');
const btnHeroIcon = document.getElementById('btnHeroIcon');
const btnHeroText = document.getElementById('btnHeroText');
const btnSidebarAddPlist = document.getElementById('btnSidebarAddPlist');
const sidebarAddPlistBox = document.getElementById('sidebarAddPlistBox');
const sidebarPlistSelect = document.getElementById('sidebarPlistSelect');
const btnConfirmAddPlist = document.getElementById('btnConfirmAddPlist');
const btnCancelAddPlist = document.getElementById('btnCancelAddPlist');
const btnSidebarMapDisc = document.getElementById('btnSidebarMapDisc');
const btnSidebarDownload = document.getElementById('btnSidebarDownload');
const btnSidebarDelete = document.getElementById('btnSidebarDelete');
const btnTrackDrawerClose = document.getElementById('btnTrackDrawerClose');

const plistDrawer = document.getElementById('plistDrawer');
const dPlistName = document.getElementById('dPlistName');
const inputRenamePlist = document.getElementById('inputRenamePlist');
const btnSavePlistRename = document.getElementById('btnSavePlistRename');
const btnMapPlistDisc = document.getElementById('btnMapPlistDisc');
const btnDeletePlist = document.getElementById('btnDeletePlist');
const btnPlistDrawerClose = document.getElementById('btnPlistDrawerClose');

const mapModal = document.getElementById('mapModal');
const modalUidVal = document.getElementById('modalUidVal');
const modalLabelInput = document.getElementById('modalLabelInput');
const modalTypeSelect = document.getElementById('modalTypeSelect');
const modalTargetLabel = document.getElementById('modalTargetLabel');
const modalTargetSelect = document.getElementById('modalTargetSelect');
const btnMapModalClose = document.getElementById('btnMapModalClose');
const btnModalSave = document.getElementById('btnModalSave');

function showToast(msg, type = 'error'){
  const toast = document.getElementById('toast');
  if(!toast) return;
  toast.textContent = msg;
  toast.className = 'toast ' + (type === 'error' ? 'toast-error' : 'toast-success');
  toast.style.display = 'block';
  if(toastTimer) clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    toast.style.display = 'none';
  }, 4000);
}

function fmt(b){
  if(b < 1048576) return (b/1024).toFixed(1) + ' KB';
  if(b < 1073741824) return (b/1048576).toFixed(1) + ' MB';
  return (b/1073741824).toFixed(1) + ' GB';
}

function cleanName(n){ return n ? n.replace(/\.[^/.]+$/, '') : ''; }

function fmtUptime(s){
  if(s < 60) return s + 's';
  const m = Math.floor(s / 60);
  const remS = s % 60;
  if(m < 60) return m + 'm ' + remS + 's';
  const h = Math.floor(m / 60);
  const remM = m % 60;
  return h + 'h ' + remM + 'm';
}

function getQueryTab(){
  const params = new URLSearchParams(window.location.search);
  return params.get('tab') || 'tracks';
}

function setQueryTab(t){
  const url = new URL(window.location);
  url.searchParams.set('tab', t);
  window.history.replaceState({}, '', url);
}

function switchView(viewName, pushState = true){
  if(pushState) setQueryTab(viewName);
  tabTracks.classList.toggle('active', viewName === 'tracks');
  tabDiscs.classList.toggle('active', viewName === 'discs');
  tabPlaylists.classList.toggle('active', viewName === 'playlists');
  tabFiles.classList.toggle('active', viewName === 'files');
  viewTracks.classList.toggle('active', viewName === 'tracks');
  viewDiscs.classList.toggle('active', viewName === 'discs');
  viewPlaylists.classList.toggle('active', viewName === 'playlists');
  viewFiles.classList.toggle('active', viewName === 'files');
  if(viewName === 'files') loadFolder(currentFolder, false);
  if(viewName === 'discs') loadDiscs();
  if(viewName === 'playlists' && !currentPlaylist) loadPlaylists();
  if(viewName === 'tracks' && !tracks.length) loadLibrary(false);
}

tabTracks.onclick = () => switchView('tracks');
tabDiscs.onclick = () => switchView('discs');
tabPlaylists.onclick = () => switchView('playlists');
tabFiles.onclick = () => switchView('files');

function closeAllDrawers(){
  drawerOverlay.classList.remove('open');
  trackDrawer.classList.remove('open');
  plistDrawer.classList.remove('open');
  mapModal.classList.remove('open');
  sidebarAddPlistBox.style.display = 'none';
}

drawerOverlay.onclick = closeAllDrawers;
btnTrackDrawerClose.onclick = closeAllDrawers;
btnPlistDrawerClose.onclick = closeAllDrawers;
btnMapModalClose.onclick = closeAllDrawers;

function fetchNow(){
  if(isFetchingNow) return;
  isFetchingNow = true;
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
        btnReaderPlay.textContent = '\u275A\u275A';
      } else if(playerState === 'paused'){
        btnAssignPrompt.style.display = 'none';
        discDot.style.background = 'var(--amber)';
        const title = d.rfid && d.rfid.present && d.rfid.label ? d.rfid.label : cleanName(d.player.name || playingPath.split('/').pop());
        readerHeadline.textContent = 'Paused: ' + title;
        readerSub.textContent = 'reader paused';
        btnReaderPlay.style.display = 'flex';
        btnReaderPlay.textContent = '\u25B6';
      } else {
        btnAssignPrompt.style.display = 'none';
        discDot.style.background = 'var(--amber)';
        readerHeadline.textContent = 'Waiting for disc...';
        readerSub.textContent = 'reader ready';
        btnReaderPlay.style.display = 'none';
      }

      if(viewTracks.classList.contains('active')) renderTracks();
      if(currentPlaylist && viewPlaylists.classList.contains('active')) renderPlaylistTracks();
      if(viewDiscs.classList.contains('active')) renderDiscs();
      updateSidebarPlayButton();
    })
    .catch(() => {
      if(btnRefresh) btnRefresh.classList.remove('spin');
      discDot.style.background = 'var(--red)';
      readerHeadline.textContent = 'Device Unreachable';
      readerSub.textContent = 'offline';
      btnReaderPlay.style.display = 'none';
      btnAssignPrompt.style.display = 'none';
    })
    .finally(() => {
      isFetchingNow = false;
      setTimeout(fetchNow, 2000);
    });
}

btnRefresh.onclick = () => fetchNow();
btnAssignPrompt.onclick = () => openMapModal(activeTagUid, 'track', '', '');
btnReaderPlay.onclick = () => {
  if(!playingPath) return;
  toggleTrackPlay(playingPath);
};

function toggleTrackPlay(path){
  fetch('/api/player/toggle?path=' + encodeURIComponent(path), { method: 'POST' })
    .then(async r => {
      const d = await r.json().catch(() => ({}));
      if(!r.ok || d.status !== 'ok'){
        showToast(d.error || 'Playback request failed');
      } else {
        fetchNow();
      }
    }).catch(e => {
      showToast('Playback error: ' + (e.message || 'connection failed'));
    });
}

function updateSidebarPlayButton(){
  if(!activeSelectedTrack) return;
  const isPlaying = (playingPath === activeSelectedTrack.path && playerState === 'playing');
  const isPaused = (playingPath === activeSelectedTrack.path && playerState === 'paused');
  if(isPlaying){
    dHeroPlayState.textContent = 'NOW PLAYING';
    dHeroPlayState.className = 'drawer-hero-state playing';
    btnHeroIcon.textContent = '\u275A\u275A';
    btnHeroText.textContent = 'Pause';
  } else if(isPaused){
    dHeroPlayState.textContent = 'PAUSED';
    dHeroPlayState.className = 'drawer-hero-state paused';
    btnHeroIcon.textContent = '\u25B6';
    btnHeroText.textContent = 'Resume';
  } else {
    dHeroPlayState.textContent = 'READY';
    dHeroPlayState.className = 'drawer-hero-state';
    btnHeroIcon.textContent = '\u25B6';
    btnHeroText.textContent = 'Play';
  }
}

function renderTracks(){
  trackList.innerHTML = '';
  if(!tracks.length){
    trackList.innerHTML = '<div class="msg">No tracks found.</div>';
    btnLoadMore.style.display = 'none';
    return;
  }
  tracks.forEach((t, i) => {
    const row = document.createElement('div');
    row.className = 'track-row';
    row.onclick = () => openTrackDrawer(t);

    const num = document.createElement('span');
    num.className = 'num';
    num.textContent = String(i + 1).padStart(2, '0');

    const main = document.createElement('div');
    main.className = 'track-main';
    const title = document.createElement('div');
    title.className = 'track-title';
    title.textContent = cleanName(t.name);
    main.appendChild(title);

    const actions = document.createElement('div');
    actions.className = 'track-actions';
    const playBtn = document.createElement('button');
    playBtn.className = 'btn-icon';
    const isPlaying = (playingPath === t.path && playerState === 'playing');
    playBtn.textContent = isPlaying ? '\u275A\u275A' : '\u25B6';
    playBtn.onclick = (e) => {
      e.stopPropagation();
      toggleTrackPlay(t.path);
    };
    actions.appendChild(playBtn);

    row.appendChild(num);
    row.appendChild(main);
    row.appendChild(actions);
    trackList.appendChild(row);
  });

  if(tracksHasMore){
    btnLoadMore.style.display = 'block';
    btnLoadMore.textContent = `Load More (${tracksTotal - tracks.length} remaining)`;
  } else {
    btnLoadMore.style.display = 'none';
  }
}

btnLoadMore.onclick = () => {
  tracksOffset += tracksLimit;
  loadLibrary(true);
};

function loadLibrary(append = false){
  if(!append){
    tracksOffset = 0;
    tracks = [];
    trackList.innerHTML = '<div class="msg">Reading library...</div>';
  }
  fetch(`/api/mp3?offset=${tracksOffset}&limit=${tracksLimit}`)
    .then(r => r.json())
    .then(d => {
      const entries = d.entries || (Array.isArray(d) ? d : []);
      tracks = append ? tracks.concat(entries) : entries;
      tracksTotal = typeof d.total === 'number' ? d.total : tracks.length;
      tracksHasMore = !!d.hasMore;
      libCount.textContent = tracksTotal + ' tracks';
      renderTracks();
    }).catch(() => {
      trackList.innerHTML = '<div class="msg">Failed to read audio files.</div>';
    });
}

function openTrackDrawer(t){
  activeSelectedTrack = t;
  dTrackName.textContent = cleanName(t.name);
  dTrackPath.textContent = t.path;
  dTrackSize.textContent = fmt(t.size);
  dTrackPlaylists.textContent = 'Loading...';
  btnSidebarDownload.href = '/api/download?path=' + encodeURIComponent(t.path);
  btnSidebarDownload.setAttribute('download', t.name);

  updateSidebarPlayButton();
  btnHeroPlay.onclick = () => toggleTrackPlay(t.path);

  drawerOverlay.classList.add('open');
  trackDrawer.classList.add('open');

  fetch('/api/track/p?path=' + encodeURIComponent(t.path))
    .then(r => r.json())
    .then(res => {
      dTrackPlaylists.innerHTML = '';
      if(!res || !res.length){
        dTrackPlaylists.textContent = 'None';
      } else {
        const wrap = document.createElement('div');
        wrap.className = 'drawer-tags';
        res.forEach(pName => {
          const sp = document.createElement('span');
          sp.className = 'drawer-tag';
          sp.textContent = pName;
          wrap.appendChild(sp);
        });
        dTrackPlaylists.appendChild(wrap);
      }
    }).catch(()=>{ dTrackPlaylists.textContent = 'Error'; });
}

btnSidebarAddPlist.onclick = () => {
  sidebarAddPlistBox.style.display = 'block';
  sidebarPlistSelect.innerHTML = '';
  playlists.forEach(p => {
    const base = p.name.replace('.m3u', '');
    const opt = document.createElement('option');
    opt.value = base;
    opt.textContent = base;
    sidebarPlistSelect.appendChild(opt);
  });
};

btnCancelAddPlist.onclick = () => {
  sidebarAddPlistBox.style.display = 'none';
};

btnConfirmAddPlist.onclick = () => {
  if(!activeSelectedTrack) return;
  const targetPlist = sidebarPlistSelect.value;
  if(!targetPlist) return;
  fetch('/api/p/tracks?id=' + encodeURIComponent(targetPlist), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: activeSelectedTrack.path })
  }).then(async r => {
    const d = await r.json().catch(() => ({}));
    if(!r.ok || d.status !== 'ok'){
      showToast(d.error || 'Failed to add track to playlist');
    } else {
      showToast('Track added to playlist', 'success');
      sidebarAddPlistBox.style.display = 'none';
      openTrackDrawer(activeSelectedTrack);
      loadPlaylists();
    }
  }).catch(e => {
    showToast('Error adding track: ' + (e.message || 'connection failed'));
  });
};

btnSidebarMapDisc.onclick = () => {
  if(!activeSelectedTrack) return;
  closeAllDrawers();
  openMapModal(activeTagUid || '', 'track', activeSelectedTrack.path, cleanName(activeSelectedTrack.name));
};

btnSidebarDelete.onclick = () => {
  if(!activeSelectedTrack) return;
  if(!confirm('Permanently delete ' + activeSelectedTrack.name + '?')) return;
  fetch('/api/delete?path=' + encodeURIComponent(activeSelectedTrack.path), { method: 'DELETE' })
    .then(async r => {
      const d = await r.json().catch(() => ({}));
      if(!r.ok || d.status !== 'ok'){
        showToast(d.error || 'Failed to delete file');
      } else {
        showToast('File deleted', 'success');
        closeAllDrawers();
        loadLibrary(false);
        if(viewFiles.classList.contains('active')) loadFolder(currentFolder, false);
      }
    }).catch(e => {
      showToast('Delete error: ' + (e.message || 'request failed'));
    });
};

function loadPlaylists(){
  fetch('/api/p').then(r=>r.json()).then(d=>{
    playlists = d;
    plistCount.textContent = d.length + ' playlists';
    plistCards.innerHTML = '';
    if(!d.length){
      plistCards.innerHTML = '<div class="msg">No playlists yet.</div>';
      return;
    }
    d.forEach(p => {
      const base = p.name.replace('.m3u','');
      const card = document.createElement('div');
      card.className = 'plist-card';
      card.onclick = () => openPlaylist(base);

      const title = document.createElement('span');
      title.className = 'plist-title';
      title.textContent = base;

      const meta = document.createElement('span');
      meta.className = 'plist-meta';
      meta.textContent = p.trackCount + ' tracks';

      card.appendChild(title);
      card.appendChild(meta);
      plistCards.appendChild(card);
    });
  }).catch(()=>{
    plistCards.innerHTML = '<div class="msg">Failed to load playlists.</div>';
  });
}

btnNewList.onclick = () => {
  if(playlists.length >= 10){
    showToast('Maximum 10 playlists reached');
    return;
  }
  newPlistForm.style.display = 'flex';
  newPlistNameInput.value = '';
  newPlistNameInput.focus();
};
btnCancelNewPlist.onclick = () => { newPlistForm.style.display = 'none'; };
btnConfirmNewPlist.onclick = () => {
  if(playlists.length >= 10){
    showToast('Maximum 10 playlists reached');
    return;
  }
  const n = newPlistNameInput.value.trim();
  if(!n) return;
  fetch('/api/p', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: n})
  }).then(async r => {
    const d = await r.json().catch(() => ({}));
    if(!r.ok || d.status !== 'ok'){
      showToast(d.error || ('Failed to create playlist (' + r.status + ')'));
    } else {
      showToast('Playlist created', 'success');
      newPlistForm.style.display = 'none';
      loadPlaylists();
    }
  }).catch(e => {
    showToast('Network error: ' + (e.message || 'could not create playlist'));
  });
};

function openPlaylist(name){
  currentPlaylist = name;
  plistMain.style.display = 'none';
  plistDetail.style.display = 'block';
  plistHeadTitle.textContent = name;
  plistDetailCount.textContent = 'Loading...';
  detailTrackList.innerHTML = '<div class="msg">Loading tracks...</div>';
  fetch('/api/p/' + encodeURIComponent(name) + '/tracks')
    .then(r=>r.json())
    .then(list => {
      currentPlaylistTracks = list;
      renderPlaylistTracks();
    }).catch(()=>{ detailTrackList.innerHTML = '<div class="msg">Failed to load tracks.</div>'; });
}

function removeTrackFromCurrentPlaylist(index){
  if(!currentPlaylist || index < 0 || index >= currentPlaylistTracks.length) return;
  const newTracks = currentPlaylistTracks.filter((_, idx) => idx !== index).map(t => t.path);
  fetch('/api/p/tracks?id=' + encodeURIComponent(currentPlaylist), {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ tracks: newTracks })
  }).then(async r => {
    const d = await r.json().catch(() => ({}));
    if(!r.ok || d.status !== 'ok'){
      showToast(d.error || 'Failed to remove track');
    } else {
      showToast('Track removed', 'success');
      openPlaylist(currentPlaylist);
    }
  }).catch(e => {
    showToast('Error removing track: ' + (e.message || 'connection failed'));
  });
}

function renderPlaylistTracks(){
  plistDetailCount.textContent = currentPlaylistTracks.length + ' tracks';
  detailTrackList.innerHTML = '';
  if(!currentPlaylistTracks.length){
    detailTrackList.innerHTML = '<div class="msg">Empty playlist.</div>';
    return;
  }
  currentPlaylistTracks.forEach((t, i) => {
    const row = document.createElement('div');
    row.className = 'track-row';
    row.onclick = () => openTrackDrawer(t);

    const num = document.createElement('span');
    num.className = 'num';
    num.textContent = String(i + 1).padStart(2, '0');

    const main = document.createElement('div');
    main.className = 'track-main';
    const title = document.createElement('div');
    title.className = 'track-title';
    title.textContent = cleanName(t.name);
    main.appendChild(title);

    const actions = document.createElement('div');
    actions.className = 'track-actions';

    const playBtn = document.createElement('button');
    playBtn.className = 'btn-icon';
    const isPlaying = (playingPath === t.path && playerState === 'playing');
    playBtn.textContent = isPlaying ? '\u275A\u275A' : '\u25B6';
    playBtn.onclick = (e) => {
      e.stopPropagation();
      toggleTrackPlay(t.path);
    };

    const delBtn = document.createElement('button');
    delBtn.className = 'btn-icon btn-del';
    delBtn.textContent = '\u2715';
    delBtn.onclick = (e) => {
      e.stopPropagation();
      removeTrackFromCurrentPlaylist(i);
    };

    actions.appendChild(playBtn);
    actions.appendChild(delBtn);

    row.appendChild(num);
    row.appendChild(main);
    row.appendChild(actions);
    detailTrackList.appendChild(row);
  });
}

btnBackPlaylists.onclick = () => {
  currentPlaylist = null;
  plistDetail.style.display = 'none';
  plistMain.style.display = 'block';
  loadPlaylists();
};

btnOpenPlistSettings.onclick = () => {
  if(!currentPlaylist) return;
  dPlistName.textContent = currentPlaylist;
  inputRenamePlist.value = currentPlaylist;
  drawerOverlay.classList.add('open');
  plistDrawer.classList.add('open');
};

btnSavePlistRename.onclick = () => {
  const n = inputRenamePlist.value.trim();
  if(!n || n === currentPlaylist) return;
  fetch('/api/p/' + encodeURIComponent(currentPlaylist), {
    method: 'PUT',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({newName: n})
  }).then(async r => {
    const d = await r.json().catch(() => ({}));
    if(!r.ok || d.status !== 'ok'){
      showToast(d.error || 'Failed to rename playlist');
    } else {
      showToast('Playlist renamed', 'success');
      currentPlaylist = n;
      plistHeadTitle.textContent = n;
      closeAllDrawers();
      loadPlaylists();
    }
  }).catch(e => {
    showToast('Rename error: ' + (e.message || 'connection failed'));
  });
};

btnMapPlistDisc.onclick = () => {
  if(!currentPlaylist) return;
  closeAllDrawers();
  openMapModal(activeTagUid || '', 'playlist', currentPlaylist, currentPlaylist);
};

btnDeletePlist.onclick = () => {
  if(!confirm('Permanently delete playlist ' + currentPlaylist + '?')) return;
  fetch('/api/p/' + encodeURIComponent(currentPlaylist), { method: 'DELETE' })
    .then(async r => {
      const d = await r.json().catch(() => ({}));
      if(!r.ok || d.status !== 'ok'){
        showToast(d.error || 'Failed to delete playlist');
      } else {
        showToast('Playlist deleted', 'success');
        closeAllDrawers();
        btnBackPlaylists.click();
      }
    }).catch(e => {
      showToast('Delete error: ' + (e.message || 'connection failed'));
    });
};

function renderBreadcrumbs(){
  fileBreadcrumbs.innerHTML = '';
  const root = document.createElement('span');
  root.className = 'bc-item';
  root.textContent = 'root';
  root.onclick = () => loadFolder('/', false);
  fileBreadcrumbs.appendChild(root);

  if(currentFolder === '/') return;
  const segments = currentFolder.split('/').filter(Boolean);
  let built = '';
  segments.forEach(seg => {
    built += '/' + seg;
    const slash = document.createElement('span');
    slash.textContent = ' / ';
    const item = document.createElement('span');
    item.className = 'bc-item';
    item.textContent = seg;
    const dest = built;
    item.onclick = () => loadFolder(dest, false);
    fileBreadcrumbs.appendChild(slash);
    fileBreadcrumbs.appendChild(item);
  });
}

function loadFolder(path, append = false){
  currentFolder = path;
  renderBreadcrumbs();
  if(!append){
    filesOffset = 0;
    browserFileList.innerHTML = '<div class="msg">Loading files...</div>';
  }
  fetch(`/api/tree?path=${encodeURIComponent(path)}&offset=${filesOffset}&limit=${filesLimit}`)
    .then(r => r.json())
    .then(d => {
      if(!append) browserFileList.innerHTML = '';
      const nodes = d.entries || (Array.isArray(d) ? d : []);
      filesHasMore = !!d.hasMore;

      if(!nodes.length && !append){
        browserFileList.innerHTML = '<div class="msg">Folder is empty.</div>';
        btnFilesLoadMore.style.display = 'none';
        return;
      }
      nodes.sort((a,b) => (b.type === 'dir') - (a.type === 'dir'));
      nodes.forEach(n => {
        const row = document.createElement('div');
        row.className = 'file-row';
        const full = (currentFolder === '/' ? '' : currentFolder) + '/' + n.name;

        const nameEl = document.createElement('div');
        nameEl.className = 'file-name ' + (n.type === 'dir' ? 'is-dir' : '');
        nameEl.textContent = (n.type === 'dir' ? '\u25B8 ' : '') + n.name;

        const sizeEl = document.createElement('div');
        sizeEl.className = 'file-size';
        sizeEl.textContent = n.type === 'dir' ? '-' : fmt(n.size);

        const actions = document.createElement('div');
        actions.className = 'track-actions';
        const delBtn = document.createElement('button');
        delBtn.className = 'btn-icon btn-del';
        delBtn.textContent = '\u2715';
        delBtn.onclick = (e) => {
          e.stopPropagation();
          const targetType = n.type === 'dir' ? 'empty folder' : 'file';
          if(!confirm('Delete ' + targetType + ' ' + full + '?')) return;
          fetch('/api/delete?path=' + encodeURIComponent(full), { method: 'DELETE' })
            .then(async r => {
              const res = await r.json().catch(() => ({}));
              if(!r.ok || res.status !== 'ok'){
                showToast(res.error || 'Failed to delete item');
              } else {
                showToast('Deleted ' + n.name, 'success');
                loadFolder(currentFolder, false);
              }
            }).catch(err => {
              showToast('Delete error: ' + (err.message || 'connection failed'));
            });
        };
        actions.appendChild(delBtn);

        row.onclick = () => {
          if(n.type === 'dir'){
            loadFolder(full, false);
          } else if(n.name.toLowerCase().endsWith('.mp3')){
            openTrackDrawer({ name: n.name, path: full, size: n.size });
          }
        };

        row.appendChild(nameEl);
        row.appendChild(sizeEl);
        row.appendChild(actions);
        browserFileList.appendChild(row);
      });

      btnFilesLoadMore.style.display = filesHasMore ? 'block' : 'none';
    }).catch(()=>{ browserFileList.innerHTML = '<div class="msg">Error reading directory.</div>'; });
}

btnFilesLoadMore.onclick = () => {
  filesOffset += filesLimit;
  loadFolder(currentFolder, true);
};

btnNewFolder.onclick = () => {
  newFolderForm.style.display = 'flex';
  newFolderNameInput.value = '';
  newFolderNameInput.focus();
};
btnCancelNewFolder.onclick = () => { newFolderForm.style.display = 'none'; };
btnConfirmNewFolder.onclick = () => {
  const n = newFolderNameInput.value.trim();
  if(!n) return;
  const p = (currentFolder === '/' ? '' : currentFolder) + '/' + n;
  fetch('/api/folders', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: p })
  }).then(async r => {
    const d = await r.json().catch(() => ({}));
    if(!r.ok || d.status !== 'ok'){
      showToast(d.error || ('Failed to create folder (' + r.status + ')'));
    } else {
      showToast('Folder created', 'success');
      newFolderForm.style.display = 'none';
      loadFolder(currentFolder, false);
    }
  }).catch(e => {
    showToast('Network error: ' + (e.message || 'could not create folder'));
  });
};

btnUp.onclick = () => fileInput.click();
fileInput.onchange = () => {
  if(!fileInput.files || fileInput.files.length === 0) return;
  const file = fileInput.files[0];
  if(file.size > 104857600){
    showToast('Exceeds 100MB limit.');
    fileInput.value = '';
    return;
  }
  pBox.style.display = 'block';
  pBar.style.width = '0%';
  pTxt.textContent = '0%';
  btnUp.disabled = true;

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload?path=' + encodeURIComponent(currentFolder));
  xhr.upload.onprogress = e => {
    if(e.lengthComputable){
      const pct = Math.round((e.loaded / e.total) * 100);
      pBar.style.width = pct + '%';
      pTxt.textContent = pct === 100 ? 'Writing...' : pct + '%';
    }
  };
  xhr.onload = () => {
    pBox.style.display = 'none';
    btnUp.disabled = false;
    fileInput.value = '';
    if(xhr.status >= 200 && xhr.status < 300){
      showToast('File uploaded successfully', 'success');
      loadFolder(currentFolder, false);
      loadLibrary(false);
    } else {
      let msg = 'Upload failed (' + xhr.status + ')';
      try {
        const res = JSON.parse(xhr.responseText);
        if(res.error) msg = 'Upload error: ' + res.error;
      } catch(_) {}
      showToast(msg);
      loadFolder(currentFolder, false);
    }
  };
  xhr.onerror = () => {
    pBox.style.display = 'none';
    btnUp.disabled = false;
    fileInput.value = '';
    showToast('Upload network connection error');
  };
  const form = new FormData();
  form.append('file', file, file.name);
  xhr.send(form);
};

function loadDiscs(){
  fetch('/api/discs').then(r=>r.json()).then(d=>{
    discs = d;
    discsCount.textContent = d.length + ' discs';
    renderDiscs();
  }).catch(()=>{ discsList.innerHTML = '<div class="msg">Failed to load discs.</div>'; });
}

function renderDiscs(){
  discsList.innerHTML = '';
  if(activeTagUid && !activeTagMapped){
    const banner = document.createElement('div');
    banner.className = 'disc-row present';
    const bLeft = document.createElement('div');
    bLeft.className = 'disc-main';
    const bSub = document.createElement('div');
    bSub.textContent = 'ACTIVE UNMAPPED TAG';
    bSub.className = 'disc-sub';
    const bVal = document.createElement('div');
    bVal.className = 'disc-title';
    bVal.textContent = activeTagUid;
    bLeft.appendChild(bSub);
    bLeft.appendChild(bVal);

    const bBtn = document.createElement('button');
    bBtn.className = 'btn-icon';
    bBtn.style.color = 'var(--amber)';
    bBtn.textContent = 'Map Disc';
    bBtn.onclick = () => openMapModal(activeTagUid, 'track', '', '');
    banner.appendChild(bLeft);
    banner.appendChild(bBtn);
    discsList.appendChild(banner);
  }

  if(!discs.length && !activeTagUid){
    discsList.innerHTML = '<div class="msg">No discs mapped yet.</div>';
    return;
  }

  discs.forEach(d => {
    const row = document.createElement('div');
    row.className = 'disc-row' + (activeTagUid === d.uid ? ' present' : '');
    row.onclick = () => openMapModal(d.uid, d.type, d.target, d.label || '');

    const main = document.createElement('div');
    main.className = 'disc-main';
    const title = document.createElement('div');
    title.className = 'disc-title';
    title.textContent = d.label || d.target;

    const sub = document.createElement('div');
    sub.className = 'disc-sub';
    const badge = document.createElement('span');
    badge.className = 'badge-type';
    badge.textContent = d.type;
    const uidSpan = document.createElement('span');
    uidSpan.style.color = 'var(--dim)';
    uidSpan.style.marginRight = '8px';
    uidSpan.textContent = d.uid;
    const targetSpan = document.createElement('span');
    targetSpan.textContent = d.target;

    sub.appendChild(badge);
    sub.appendChild(uidSpan);
    sub.appendChild(targetSpan);
    main.appendChild(title);
    main.appendChild(sub);

    const actions = document.createElement('div');
    actions.className = 'track-actions';

    const delBtn = document.createElement('button');
    delBtn.className = 'btn-icon btn-del';
    delBtn.textContent = '\u2715';
    delBtn.onclick = (e) => {
      e.stopPropagation();
      if(!confirm('Unmap disc ' + d.uid + '?')) return;
      fetch('/api/discs?uid=' + encodeURIComponent(d.uid), { method: 'DELETE' })
        .then(async r => {
          const res = await r.json().catch(() => ({}));
          if(!r.ok || res.status !== 'ok'){
            showToast(res.error || 'Failed to unmap disc');
          } else {
            showToast('Disc unmapped', 'success');
            loadDiscs();
            fetchNow();
          }
        }).catch(err => {
          showToast('Error unmapping disc: ' + (err.message || 'request failed'));
        });
    };

    actions.appendChild(delBtn);
    row.appendChild(main);
    row.appendChild(actions);
    discsList.appendChild(row);
  });
}

function openMapModal(uid, type = 'track', target = '', label = ''){
  if(!tracks.length) loadLibrary(false);
  if(!playlists.length) loadPlaylists();

  modalUidVal.textContent = uid;
  modalLabelInput.value = label;
  modalTypeSelect.value = type;

  populateModalTargets(type, target);
  drawerOverlay.classList.add('open');
  mapModal.classList.add('open');
}

modalTypeSelect.onchange = () => populateModalTargets(modalTypeSelect.value);

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
      opt.textContent = base;
      if(base === selected) opt.selected = true;
      modalTargetSelect.appendChild(opt);
    });
  }
}

btnModalSave.onclick = () => {
  const uid = modalUidVal.textContent;
  const label = modalLabelInput.value.trim();
  const type = modalTypeSelect.value;
  const target = modalTargetSelect.value;
  if(!target) {
    showToast('Please select a target for mapping');
    return;
  }

  fetch('/api/discs/map', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ uid, type, target, label })
  }).then(async r => {
    const d = await r.json().catch(() => ({}));
    if(!r.ok || d.status !== 'ok'){
      showToast(d.error || 'Failed to map disc');
    } else {
      showToast('Disc mapped successfully', 'success');
      closeAllDrawers();
      loadDiscs();
      fetchNow();
    }
  }).catch(e => {
    showToast('Mapping error: ' + (e.message || 'connection failed'));
  });
};

switchView(getQueryTab(), false);
fetchNow();
loadLibrary(false);
loadPlaylists();
loadDiscs();
)JS";
