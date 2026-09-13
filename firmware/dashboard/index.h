#pragma once
#include "dashboard.js.h"
#include "files.h"
#include "playlists.h"
#include "sidebar.h"
#include "styles.h"
#include "tracks.h"
#include <Arduino.h>
#include <WebServer.h>

inline void streamDashboard(WebServer &srv) {
	srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
	srv.send(200, "text/html", "");

	srv.sendContent_P(PSTR("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
						   "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
						   "<title>Smallben</title><style>"));
	srv.sendContent_P(DASHBOARD_CSS);
	srv.sendContent_P(PSTR(
		"</style></head><body><div class=\"wrap\">"
		"<div class=\"top\"><div class=\"brand\">Smallben</div>"
		"<div class=\"status\"><span class=\"sys-info\" id=\"sysInfo\"></span></div></div>"
		"<div class=\"reader-bar\"><div class=\"disc-indicator\"><div class=\"disc-dot\" "
		"id=\"discDot\"></div></div>"
		"<div class=\"reader-text\"><div class=\"reader-sub\" id=\"readerSub\">reader status</div>"
		"<div class=\"reader-headline\" id=\"readerHeadline\">Waiting for disc...</div></div>"
		"<div class=\"reader-controls\">"
		"<button class=\"btn-icon\" id=\"btnAssignPrompt\" "
		"style=\"display:none;background:var(--amber);color:#121110;font-weight:bold;border-radius:"
		"2px;padding:6px 10px\">Assign</button>"
		"<button class=\"btn-reader-action\" id=\"btnReaderPlay\">&#9654;</button>"
		"<button class=\"btn-refresh\" id=\"btnRefresh\" title=\"Refresh Status\">&#x21bb;</button>"
		"</div></div><div class=\"nav-tabs\">"
		"<button class=\"nav-tab active\" id=\"tabTracks\">Tracks</button>"
		"<button class=\"nav-tab\" id=\"tabPlaylists\">Playlists</button>"
		"<button class=\"nav-tab\" id=\"tabDiscs\">Discs</button>"
		"<button class=\"nav-tab\" id=\"tabFiles\">Files</button></div>"));

	srv.sendContent_P(DASHBOARD_TRACKS_HTML);
	srv.sendContent_P(DASHBOARD_PLAYLISTS_HTML);

	srv.sendContent_P(
		PSTR("<div id=\"viewDiscs\" class=\"view\"><div class=\"head\">"
			 "<div class=\"title\">Discs Registry</div><div class=\"count\" "
			 "id=\"discsCount\">...</div></div>"
			 "<div id=\"discsList\"><div class=\"msg\">Reading discs...</div></div></div>"));

	srv.sendContent_P(DASHBOARD_FILES_HTML);
	srv.sendContent_P(PSTR("</div>"));

	srv.sendContent_P(DASHBOARD_SIDEBAR_HTML);
	srv.sendContent_P(PSTR("<div id=\"toast\" class=\"toast\"></div>"));

	srv.sendContent_P(PSTR("<script>"));
	srv.sendContent_P(DASHBOARD_JS);
	srv.sendContent_P(PSTR("</script></body></html>"));
	srv.sendContent("");
}
