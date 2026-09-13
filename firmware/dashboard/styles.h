#pragma once
#include <pgmspace.h>

const char DASHBOARD_CSS[] PROGMEM = R"CSS(
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
.track-row{display:grid;grid-template-columns:30px 1fr auto;gap:12px;padding:12px 10px;border-bottom:1px solid var(--line);align-items:center;cursor:pointer;transition:background .15s;border-radius:2px}
.track-row:hover{background:rgba(255,255,255,0.03)}
.num{color:var(--dim);font-size:12px;font-variant-numeric:tabular-nums}
.track-main{min-width:0}
.track-title{color:var(--text);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.track-actions{display:flex;gap:6px}
.btn-icon{background:none;border:none;color:var(--dim);cursor:pointer;padding:6px 8px;font-family:inherit;font-size:12px;transition:color .15s;display:inline-flex;align-items:center;justify-content:center}
.btn-icon:hover{color:var(--amber)}
.btn-del:hover{color:var(--red)}
.btn-loadmore{width:100%;padding:12px;background:var(--panel);border:1px solid var(--line);color:var(--amber);font-family:inherit;font-size:12px;cursor:pointer;text-align:center;transition:background .15s,border-color .15s;margin-top:16px;border-radius:2px}
.btn-loadmore:hover{background:var(--panel-hover);border-color:var(--dim)}
.plist-card{display:flex;justify-content:space-between;align-items:center;padding:18px 20px;background:var(--panel);border:1px solid var(--line);border-radius:2px;margin-bottom:10px;cursor:pointer;transition:background .15s,border-color .15s}
.plist-card:hover{background:var(--panel-hover);border-color:var(--dim)}
.plist-title{color:var(--text);font-size:14px;font-weight:400}
.plist-meta{color:var(--muted);font-size:12px;font-variant-numeric:tabular-nums}
.btn-build{width:100%;padding:16px;background:transparent;border:1px dashed var(--line);color:var(--muted);font-family:inherit;font-size:12px;cursor:pointer;text-align:center;transition:border-color .15s,color .15s;margin-top:12px;border-radius:2px}
.btn-build:hover{border-color:var(--dim);color:var(--text)}
.bar-wrap{display:flex;justify-content:space-between;align-items:center;margin-bottom:16px}
.btn-link{background:none;border:none;color:var(--amber);cursor:pointer;font-family:inherit;font-size:12px;padding:0}
.btn-link:hover{text-decoration:underline}
.breadcrumbs{display:flex;align-items:center;gap:6px;flex-wrap:wrap;font-size:12px;color:var(--dim);margin-bottom:12px;padding:6px 0}
.bc-item{color:var(--amber);cursor:pointer}
.bc-item:hover{text-decoration:underline}
.file-list{background:var(--panel);border:1px solid var(--line);border-radius:2px}
.file-row{display:grid;grid-template-columns:1fr auto auto;gap:12px;padding:12px 16px;border-bottom:1px solid var(--line);align-items:center;cursor:pointer;transition:background .15s}
.file-row:last-child{border-bottom:none}
.file-row:hover{background:rgba(255,255,255,0.03)}
.file-name{display:flex;align-items:center;gap:8px;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:var(--text)}
.file-name.is-dir{color:var(--amber)}
.file-size{font-size:11px;color:var(--muted);font-variant-numeric:tabular-nums}
.progress-box{display:none;width:100%;background:var(--panel);border:1px solid var(--line);height:14px;margin-bottom:16px;position:relative}
.progress-bar{height:100%;width:0%;background:var(--amber);transition:width .1s}
.progress-txt{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:9px;color:var(--text);font-weight:bold}
.msg{color:var(--dim);padding:20px 0;text-align:center}
.drawer-overlay{position:fixed;inset:0;background:rgba(0,0,0,0.6);opacity:0;pointer-events:none;transition:opacity .2s ease;z-index:90}
.drawer-overlay.open{opacity:1;pointer-events:auto}
.drawer{position:fixed;top:0;right:0;bottom:0;width:380px;max-width:92vw;background:var(--panel);border-left:1px solid var(--line);box-shadow:-6px 0 28px rgba(0,0,0,0.6);transform:translateX(100%);transition:transform .25s cubic-bezier(0.16,1,0.3,1);z-index:100;display:flex;flex-direction:column;padding:24px;overflow-y:auto}
.drawer.open{transform:translateX(0)}
.drawer-top{display:flex;justify-content:space-between;align-items:center;margin-bottom:16px}
.drawer-heading{font-family:var(--font-serif);font-size:18px;font-weight:600}
.drawer-close{background:none;border:none;color:var(--muted);font-size:16px;cursor:pointer;padding:4px}
.drawer-close:hover{color:var(--text)}
.drawer-hero{background:var(--bg);border:1px solid var(--line);padding:14px;border-radius:3px;display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px}
.drawer-hero-info{min-width:0;flex-grow:1}
.drawer-hero-state{font-size:9px;color:var(--dim);letter-spacing:0.06em;font-weight:bold}
.drawer-hero-state.playing{color:var(--green)}
.drawer-hero-state.paused{color:var(--amber)}
.drawer-hero-btn{background:var(--panel);border:1px solid var(--line);color:var(--text);padding:8px 14px;border-radius:2px;cursor:pointer;display:inline-flex;align-items:center;gap:6px;font-family:inherit;font-size:12px;transition:border-color .15s}
.drawer-hero-btn:hover{border-color:var(--amber)}
.drawer-name{font-family:var(--font-serif);font-size:15px;color:var(--text);margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.drawer-meta-list{display:flex;flex-direction:column;gap:12px;margin-bottom:16px;border-top:1px solid var(--line);border-bottom:1px solid var(--line);padding:14px 0}
.drawer-meta-item{display:flex;flex-direction:column;gap:3px}
.drawer-meta-label{font-size:9px;text-transform:uppercase;color:var(--muted);letter-spacing:0.05em}
.drawer-meta-val{font-size:12px;color:var(--text);word-break:break-all}
.drawer-tags{display:flex;flex-wrap:wrap;gap:6px;margin-top:4px}
.drawer-tag{background:var(--bg);border:1px solid var(--line);color:var(--amber);padding:2px 6px;border-radius:2px;font-size:11px}
.drawer-section-title{font-size:10px;text-transform:uppercase;color:var(--dim);letter-spacing:0.05em;margin-bottom:8px}
.drawer-btn-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.drawer-btn-group{display:flex;flex-direction:column;gap:10px}
.btn-action{display:block;width:100%;text-align:center;padding:10px 12px;border:none;cursor:pointer;font-family:inherit;font-size:12px;border-radius:2px;text-decoration:none;transition:opacity .15s}
.btn-action:hover{opacity:0.9}
.btn-primary{background:var(--amber);color:#121110;font-weight:bold}
.btn-sec{background:transparent;border:1px solid var(--line);color:var(--text)}
.btn-sec:hover{border-color:var(--dim)}
.btn-danger-link{background:transparent;border:1px solid rgba(217,93,93,0.3);color:var(--red)}
.btn-danger-link:hover{border-color:var(--red)}
.disc-row{display:grid;grid-template-columns:1fr auto;gap:12px;padding:14px 16px;background:var(--panel);border:1px solid var(--line);border-radius:2px;margin-bottom:10px;align-items:center;cursor:pointer;transition:background .15s,border-color .15s}
.disc-row:hover{background:var(--panel-hover);border-color:var(--dim)}
.disc-row.present{border-color:var(--amber)}
.disc-main{min-width:0}
.disc-title{font-size:14px;color:var(--text);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.disc-sub{font-size:11px;color:var(--muted);margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.badge-type{display:inline-block;padding:2px 6px;font-size:9px;text-transform:uppercase;border-radius:2px;background:var(--bg);border:1px solid var(--line);color:var(--amber);margin-right:6px}
.inline-form{background:var(--bg);border:1px solid var(--line);padding:14px;border-radius:2px;margin-bottom:16px;display:flex;flex-direction:column;gap:10px}
.input-text{background:var(--panel);border:1px solid var(--line);color:var(--text);padding:8px 10px;font-family:inherit;font-size:12px;border-radius:2px;outline:none;width:100%}
.input-text:focus{border-color:var(--amber)}
.form-actions{display:flex;gap:8px;justify-content:flex-end}
.toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);background:var(--panel);border:1px solid var(--line);color:var(--text);padding:10px 18px;border-radius:3px;font-size:12px;z-index:200;display:none;box-shadow:0 6px 20px rgba(0,0,0,0.6);max-width:90vw;text-align:center;pointer-events:none}
.toast.toast-error{border-color:var(--red);color:var(--red)}
.toast.toast-success{border-color:var(--green);color:var(--green)}
@media (max-width: 480px){
  .drawer{width:90vw;max-width:90vw}
}
)CSS";
