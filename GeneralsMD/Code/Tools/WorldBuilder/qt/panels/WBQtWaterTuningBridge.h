// WBQtWaterTuningBridge.h -- opaque facade for the Qt Water tuning window (File > Map.ini >
// Water tuning).
//
// The MFC side owns the key table and writes each change into the live WaterTransparency override
// or GameData, so the 3D view shows it. The Qt side owns the rows and the WaterTransparency and
// GameData blocks of the map's map.ini.
#ifndef WB_QT_WATER_TUNING_BRIDGE_H
#define WB_QT_WATER_TUNING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	WBQT_WATER_FLOAT = 0,
	WBQT_WATER_BOOL = 1,
	WBQT_WATER_COLOR = 2,
	WBQT_WATER_TEXT = 3
};

typedef struct WBQtWaterTuningDesc
{
	const char *key;	// INI key in its block
	const char *block;	// map.ini block the key is saved in, WaterTransparency or GameData
	const char *group;	// heading over the key's rows, NULL on the water tab
	const char *help;	// tooltip
	int kind;			// WBQT_WATER_*
	float lo;
	float hi;
	float step;
	int advanced;		// 1 hides the row until "Advanced keys" is ticked
} WBQtWaterTuningDesc;

// ====== Qt -> MFC (implemented in src/WBQtWaterTuningBridge.cpp) ======

int  WBQtWaterTuning_Count(void);
int  WBQtWaterTuning_GetDesc(int i, WBQtWaterTuningDesc *out);

// A float or bool key uses v[0]. A colour uses v[0..2] as 0 to 255, negative when unset.
// GetBase reads the Water.ini or GameData.ini value under any map override.
void WBQtWaterTuning_GetBase(int i, float v[3]);
void WBQtWaterTuning_SetLive(int i, const float v[3]);
// The value in effect now, with any map override.
void WBQtWaterTuning_GetLive(int i, float v[3]);

// A text key's value. Returns 0 when i is not a text key.
int  WBQtWaterTuning_GetBaseText(int i, char *buf, int size);
void WBQtWaterTuning_SetLiveText(int i, const char *text);

// The colour tables in the game's Art/Textures, one file name per line. Returns the count.
int  WBQtWaterTuning_ListTables(char *buf, int size);

// The window saved map.ini itself, so the auto-reload watch must not treat it as an outside edit.
void WBQtWaterTuning_NoteSaved(void);

// ====== MFC only (implemented in src/WBQtWaterTuningBridge.cpp) ======

// WorldBuilder's map.ini loader skips GameData blocks, so the tuned GameData keys are applied here.
void WBQtWaterTuning_ApplyGameData(const char *iniPath);

// Returns the tuned GameData keys to their GameData.ini values.
void WBQtWaterTuning_RestoreGameData(void);

// ====== MFC -> Qt (implemented in qt/panels/WBQtWaterTuningPanel.cpp) ======

void WBQtWaterTuning_Open(void *frameHwnd, const char *iniPath);

// Another map is loading: save what is pending and hide.
void WBQtWaterTuning_MapChanged(void);

// The map.ini overrides were reloaded or dropped: read the file again while visible.
void WBQtWaterTuning_PushRefresh(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_WATER_TUNING_BRIDGE_H
