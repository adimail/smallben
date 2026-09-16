#pragma once
#include <pgmspace.h>

const char DASHBOARD_FILES_HTML[] PROGMEM = R"HTML(
<div id="viewFiles" class="view">
  <div class="head">
    <div class="title">File Storage</div>
    <div style="display:flex;gap:12px">
      <button class="btn-link" id="btnNewFolder">+ Folder</button>
      <button class="btn-link" id="btnUp">&#8593; Upload</button>
    </div>
    <input type="file" id="fileInput" style="display:none">
  </div>
  <div id="newFolderForm" class="inline-form" style="display:none">
    <input type="text" id="newFolderNameInput" class="input-text" placeholder="Folder name">
    <div class="form-actions">
      <button class="btn-icon" id="btnCancelNewFolder">Cancel</button>
      <button class="btn-icon" id="btnConfirmNewFolder" style="color:var(--amber);font-weight:bold">Create</button>
    </div>
  </div>
  <div class="breadcrumbs" id="fileBreadcrumbs"></div>
  <div class="progress-box" id="pBox">
    <div class="progress-bar" id="pBar"></div>
    <div class="progress-txt" id="pTxt">0%</div>
  </div>
  <div class="file-list" id="browserFileList"></div>
  <button class="btn-loadmore" id="btnFilesLoadMore" style="display:none">Load More</button>
</div>
)HTML";
