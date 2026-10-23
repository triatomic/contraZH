// WBQtMapIniReport.h -- native Qt viewer for the map.ini loader's report (shown on map
// open / Reload / Check). Shows the summary, any warnings, and every block the map.ini
// defines as a collapsible, syntax-coloured code view grouped by data store, with a live
// filter, per-block copy and pop-out, and Copy-to-clipboard for the whole plain-text report.
//
// The report is produced in WorldBuilderDoc.cpp (doLoadMapIni). This is a pure C facade (no
// Qt includes) so the MFC side can call it without pulling Qt headers into an MFC translation
// unit; the QDialog lives in the .cpp. The caller stages the structured report with the Add*
// calls, then calls Show, which consumes and clears it.
#ifndef WB_QT_MAPINI_REPORT_H
#define WB_QT_MAPINI_REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

// What happened to one map.ini block.
enum
{
	WBQT_MAPINI_BLOCK_LOADED = 0,	///< parsed and applied
	WBQT_MAPINI_BLOCK_OVERRIDDEN,	///< an Object that replaces an existing template
	WBQT_MAPINI_BLOCK_NEW,			///< an Object the game data doesn't define
	WBQT_MAPINI_BLOCK_DROPPED,		///< WorldBuilder couldn't parse it, so it did not apply
	WBQT_MAPINI_BLOCK_IGNORED		///< a block type WorldBuilder doesn't load; the game still does
};

enum
{
	WBQT_MAPINI_MSG_INFO = 0,
	WBQT_MAPINI_MSG_WARNING,
	WBQT_MAPINI_MSG_ERROR
};

// Drop anything staged and not shown.
void WBQtMapIniReport_Clear(void);

// One headline line (banner, counts).
void WBQtMapIniReport_AddSummary(const char *line);

// A titled note; body lines are '\n'-separated.
void WBQtMapIniReport_AddMessage(int kind, const char *title, const char *body);

// One block: its store keyword ("Object", "Weapon", ...), name (may be empty), status, the
// 1-based map.ini line it starts on, its verbatim source ('\n'-separated), and the 0-based
// lines within that source the loader blanked out.
void WBQtMapIniReport_AddBlock(const char *store, const char *name, int status, int firstLine,
	const char *source, const int *blankedLines, int blankedCount);

// Show the staged report modally, then clear it.
//   applyMode == 0: informational (Check) -- a single Close button.
//   applyMode == 1: confirm -- OK / Cancel buttons; the caller applies on OK, discards on Cancel.
// text is the plain-text report that Copy report puts on the clipboard.
// Returns: 2 = shown, user clicked OK/accepted;  1 = shown, user clicked Cancel/closed;
//          0 = Qt not available (caller falls back to the MFC dialog / its own prompt).
// All strings are local-8bit C strings.
int WBQtMapIniReport_Show(const char *title, const char *text, int applyMode);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MAPINI_REPORT_H
