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
:root{--bg:#171513;--panel:#1f1c19;--line:#322d29;--text:#efe9e0;--muted:#8c8478;--dim:#5e584f;--amber:#e8a33d;--green:#7fc99a;--red:#e06c75;
--font-serif:Georgia,'Times New Roman',serif;
--font-mono:ui-monospace,SFMono-Regular,Menlo,Consolas,'Liberation Mono',monospace;}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:var(--font-mono);font-size:13px;line-height:1.6}
.wrap{max-width:680px;margin:0 auto;padding:0 16px}
.top{display:flex;justify-content:space-between;align-items:center;padding:20px 0;border-bottom:1px solid var(--line)}
.brand{font-family:var(--font-serif);font-weight:600;font-size:22px}
.status{display:flex;align-items:center;gap:8px;font-size:12px;color:var(--muted)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 0 3px #22301f}
.hero{padding:36px 0;border-bottom:1px solid var(--line)}
.ring{width:44px;height:44px;border-radius:50%;border:1px solid var(--line);display:flex;align-items:center;justify-content:center;margin-bottom:12px}
.ring-core{width:8px;height:8px;border-radius:50%;background:var(--dim)}
.sub{font-size:11px;color:var(--dim);text-transform:uppercase;letter-spacing:.05em}
.headline{font-family:var(--font-serif);font-size:20px;margin-top:4px;color:var(--text)}
.block{padding:28px 0}
.head{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:16px}
.title{font-family:var(--font-serif);font-size:18px}
.count{color:var(--dim);font-size:12px}
.actions{display:flex;gap:8px;align-items:center;margin-bottom:16px;flex-wrap:wrap}
.tabs{display:flex;gap:8px}
.tab{background:var(--panel);border:1px solid var(--line);color:var(--muted);padding:5px 12px;font-family:inherit;font-size:12px;cursor:pointer}
.tab.active{border-color:#4a3a20;color:var(--amber);background:rgba(232,163,61,.08)}
.btn-up{background:var(--panel);border:1px solid var(--line);color:var(--text);padding:5px 12px;font-family:inherit;font-size:12px;cursor:pointer;margin-left:auto}
.btn-up:hover{border-color:var(--dim)}
.progress-box{display:none;width:100%;background:var(--panel);border:1px solid var(--line);height:18px;margin-bottom:16px;position:relative}
.progress-bar{height:100%;width:0%;background:var(--amber);transition:width .1s}
.progress-txt{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:10px;color:var(--text);font-weight:bold}
.track{display:grid;grid-template-columns:26px 1fr auto;gap:10px;padding:9px 0;border-bottom:1px solid var(--line);align-items:center}
.num{color:var(--dim);font-size:11px}
.name{color:var(--text);word-break:break-all}
.size{color:var(--muted);font-size:12px;white-space:nowrap}
.btn-del{background:none;border:none;color:var(--dim);cursor:pointer;padding:2px 6px;font-family:inherit;font-size:12px}
.btn-del:hover{color:var(--red)}
.tree{background:var(--panel);border:1px solid var(--line);padding:12px 16px}
.tree ul{list-style:none;padding-left:16px}
.tree>ul{padding-left:0}
.tree li{padding:4px 0}
.tree-row{display:flex;justify-content:space-between;align-items:center;gap:8px}
.tree-left{display:flex;align-items:center;gap:6px;min-width:0}
.folder{color:var(--amber);cursor:pointer;user-select:none;word-break:break-all}
.file-dl{color:var(--text);text-decoration:none;cursor:pointer;word-break:break-all}
.file-dl:hover{color:var(--amber);text-decoration:underline}
.tree-size{color:var(--dim);font-size:11px;white-space:nowrap}
.msg{color:var(--dim);padding:20px 0}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <div class="brand">Smallben</div>
    <div class="status"><span class="dot"></span><span>online</span></div>
  </div>
  <div class="hero">
    <div class="ring"><div class="ring-core"></div></div>
    <div class="sub">reader status</div>
    <div class="headline">Ready. Manage library and track storage below.</div>
  </div>
  <div class="block">
    <div class="head">
      <div class="title">Library</div>
      <div class="count" id="cnt">Scanning...</div>
    </div>
    <div class="actions">
      <div class="tabs">
        <button class="tab active" id="tMp3">Tracks (.mp3)</button>
        <button class="tab" id="tTree">File Tree</button>
      </div>
      <button class="btn-up" id="btnUp">&#8593; Upload File</button>
      <input type="file" id="fileInput" style="display:none">
    </div>
    <div class="progress-box" id="pBox">
      <div class="progress-bar" id="pBar"></div>
      <div class="progress-txt" id="pTxt">0%</div>
    </div>
    <div id="vMp3"><div class="msg" id="mp3List">Reading SD card...</div></div>
    <div id="vTree" style="display:none"><div class="tree" id="treeList">Loading tree...</div></div>
  </div>
</div>
<script>
const cnt=document.getElementById('cnt'),mp3List=document.getElementById('mp3List'),treeList=document.getElementById('treeList'),
vMp3=document.getElementById('vMp3'),vTree=document.getElementById('vTree'),tMp3=document.getElementById('tMp3'),tTree=document.getElementById('tTree'),
btnUp=document.getElementById('btnUp'),fileInput=document.getElementById('fileInput'),pBox=document.getElementById('pBox'),
pBar=document.getElementById('pBar'),pTxt=document.getElementById('pTxt');

let curTab='mp3';

function fmt(b){return b<1048576?(b/1024).toFixed(1)+' KB':(b/1048576).toFixed(1)+' MB'}

function showTab(mp3){
  curTab=mp3?'mp3':'tree';
  tMp3.classList.toggle('active',mp3);tTree.classList.toggle('active',!mp3);
  vMp3.style.display=mp3?'block':'none';vTree.style.display=mp3?'none':'block';
}

function refreshCurrentView(){
  if(curTab==='mp3') loadMp3();
  else loadTree();
}

function confirmDelete(path){
  if(!confirm('Are you sure you want to permanently delete:\n'+path)) return;
  fetch('/api/delete?path='+encodeURIComponent(path),{method:'DELETE'})
    .then(r=>r.json())
    .then(d=>{
      if(d.status==='ok') refreshCurrentView();
      else alert(d.error||'Failed to delete file.');
    })
    .catch(()=>alert('Network error while deleting file.'));
}

btnUp.onclick=()=>fileInput.click();

fileInput.onchange=()=>{
  if(!fileInput.files||fileInput.files.length===0) return;
  const file=fileInput.files[0];
  if(file.size>31457280){
    alert('File exceeds the 30MB size limit.');
    fileInput.value='';
    return;
  }

  pBox.style.display='block';
  pBar.style.width='0%';
  pTxt.textContent='0%';
  btnUp.disabled=true;

  const xhr=new XMLHttpRequest();
  xhr.open('POST','/api/upload?path=/');

  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){
      const pct=Math.round((e.loaded/e.total)*100);
      pBar.style.width=pct+'%';
      pTxt.textContent=pct+'%';
    }
  };

  xhr.onload=()=>{
    pBox.style.display='none';
    btnUp.disabled=false;
    fileInput.value='';
    if(xhr.status>=200&&xhr.status<300){
      refreshCurrentView();
    }else{
      alert('Upload failed: '+(xhr.responseText||'Server error'));
    }
  };

  xhr.onerror=()=>{
    pBox.style.display='none';
    btnUp.disabled=false;
    fileInput.value='';
    alert('Upload failed due to network error.');
  };

  const form=new FormData();
  form.append('file',file,file.name);
  xhr.send(form);
};

function loadMp3(){
  showTab(true);
  fetch('/api/mp3').then(r=>r.json()).then(d=>{
    cnt.textContent=d.length+' tracks';
    if(!d.length){mp3List.textContent='No MP3 files found.';return;}
    mp3List.innerHTML=d.map((f,i)=>`
      <div class="track">
        <span class="num">${String(i+1).padStart(2,'0')}</span>
        <span class="name">${f.name}</span>
        <span class="size">${fmt(f.size)}</span>
      </div>`).join('');
  }).catch(()=>{mp3List.textContent='Error reading tracks.';});
}

function fetchLevel(path){
  return fetch('/api/tree?path='+encodeURIComponent(path)).then(r=>r.json());
}

function createTreeItem(node, parentPath){
  const li=document.createElement('li');
  const full=(parentPath==='/'?'':parentPath)+'/'+node.name;
  if(node.type==='dir'){
    const row=document.createElement('div');
    row.className='tree-row';
    const left=document.createElement('div');
    left.className='tree-left';
    const label=document.createElement('span');
    label.className='folder';
    label.textContent='\u25B8 \uD83D\uDCC1 '+node.name+'/';
    left.appendChild(label);
    row.appendChild(left);
    const childUl=document.createElement('ul');
    childUl.style.display='none';
    let loaded=false, loading=false;
    label.onclick=()=>{
      const open=childUl.style.display==='block';
      if(open){ childUl.style.display='none'; label.textContent='\u25B8 \uD83D\uDCC1 '+node.name+'/'; return; }
      label.textContent='\u25BE \uD83D\uDCC1 '+node.name+'/';
      childUl.style.display='block';
      if(loaded||loading) return;
      loading=true;
      childUl.innerHTML='<li class="msg">Loading...</li>';
      fetchLevel(full).then(children=>{
        loaded=true; loading=false;
        childUl.innerHTML='';
        if(!children.length){childUl.innerHTML='<li class="msg">Empty</li>';return;}
        children.forEach(c=>childUl.appendChild(createTreeItem(c, full)));
      }).catch(()=>{loading=false; childUl.innerHTML='<li class="msg">Error loading folder</li>';});
    };
    li.appendChild(row);
    li.appendChild(childUl);
  } else {
    li.innerHTML=`<div class="tree-row">
      <div class="tree-left">
        <a class="file-dl" href="/api/download?path=${encodeURIComponent(full)}" download="${node.name}">\uD83D\uDCC4 ${node.name}</a>
        <span class="tree-size">${fmt(node.size)}</span>
      </div>
      <button class="btn-del" onclick="confirmDelete('${full.replace(/'/g,"\\'")}')">&#10005;</button>
    </div>`;
  }
  return li;
}

function loadTree(){
  showTab(false);
  treeList.innerHTML='<div class="msg">Loading tree...</div>';
  fetchLevel('/').then(nodes=>{
    treeList.innerHTML='';
    if(!nodes.length){treeList.innerHTML='<div class="msg">Empty</div>';return;}
    const ul=document.createElement('ul');
    nodes.forEach(n=>ul.appendChild(createTreeItem(n, '/')));
    treeList.appendChild(ul);
  }).catch(()=>{treeList.textContent='Error reading tree.';});
}

tMp3.onclick=loadMp3;tTree.onclick=loadTree;
loadMp3();
</script>
</body>
</html>
)HTML";
