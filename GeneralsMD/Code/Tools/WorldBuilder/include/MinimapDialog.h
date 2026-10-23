/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifndef __MINIMAP_DIALOG_H_
#define __MINIMAP_DIALOG_H_

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include <vector>

class MapObject;
struct ID2D1Factory;
struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct ID2D1Bitmap;

#define MINIMAP_SECTION "MinimapDialog"

// Configurable sampling resolution bounds. The pixel buffer is heap-allocated to
// m_resolution * m_resolution UnsignedInts, so large sizes don't bloat the object.
enum
{
	MINIMAP_RES_MIN     = 256,
	MINIMAP_RES_DEFAULT = 256,
	MINIMAP_RES_MAX     = 2048,
};

// The Minimap is a floating modeless tool window. Click/drag recenters the 3D
// viewport (in heightmap CELL units; setupCamera scales by MAP_XY_FACTOR).
class MinimapDialog : public CDialog
{
public:
	enum { IDD = IDD_MINIMAP };
	MinimapDialog(CWnd *pParent = NULL);
	virtual ~MinimapDialog();

	void rebuildTerrain();			///< Resample the terrain (expensive); then composite objects.
	void refreshObjects();			///< Cheap: re-composite cached terrain + objects (no resample).
	void requestRebuild(Bool terrainChanged = true);	///< Object/camera changes refresh instantly; terrain edits are throttled.
	void requestSelectionRefresh();	///< Selection-only change: rebuild the blip list, keep the terrain + roads bitmap.
	void requestViewBoxRefresh();	///< Camera moved: repaint only.

	// Called from the pointer tool after a click that may have changed the selection.
	// Does nothing unless the minimap is actually VISIBLE, the selection-overlay is on,
	// AND the selected set really changed since the last refresh -- so a hidden minimap,
	// the overlay-off case, and no-op clicks never touch the 3D viewport's redraw path.
	// Cheap-first: the O(1) visible/overlay checks gate the O(n) selection signature.
	static void notifySelectionChanged();

	// Suppress all minimap rebuilds while a map load/teardown is in progress. A modal
	// MessageBox during OnOpenDocument pumps messages, which can fire the pending
	// rebuild timer and run rebuildTerrain() against a half-swapped document -> hang.
	// The doc brackets the load with setLoading(true/false); the false call also kicks
	// one clean rebuild.
	static void setLoading(Bool loading);
	static Bool isLoading() { return s_loading; }

	// --- configuration (persisted to registry under MINIMAP_SECTION) ---
	void setShowObjects(Bool show);
	Bool getShowObjects() const { return m_showObjects; }

	void setShowRoads(Bool show);
	Bool getShowRoads() const { return m_showRoads; }

	void setShowBorder(Bool show);		///< Draw an orange outline at the playable-area boundary.
	Bool getShowBorder() const { return m_showBorder; }

	void setFullExtent(Bool full);		///< Map the full heightmap (playable + border) vs. playable area only.
	Bool getFullExtent() const { return m_fullExtent; }

	void setCullObjects(Bool cull);		///< Only draw object blips inside the 3D view frustum.
	Bool getCullObjects() const { return m_cullObjects; }

	void setRefreshDelayMs(Int ms);		///< 0 = manual (rebuild only on load/toggle).
	Int  getRefreshDelayMs() const { return m_refreshDelayMs; }

	void setResolution(Int res);		///< Clamped to [MINIMAP_RES_MIN, MINIMAP_RES_MAX].
	Int  getResolution() const { return m_resolution; }

protected:
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnOK();

	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnExitSizeMove();		///< Persist the window position once, when the user finishes dragging.
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	DECLARE_MESSAGE_MAP()

private:
	void interpolateColorForHeight(RGBColor *color, Real height,
		Real hiZ, Real midZ, Real loZ);
	Bool minimapToWorld(Int mx, Int my, Real *worldX, Real *worldY);
	void centerViewAtClient(CPoint point);
	void allocBuffer();				///< (Re)allocate the buffers for the current resolution.
	void buildBlips();				///< Cache the unit/structure markers Direct2D draws each paint.

	// One unit/structure marker. Positions are fractions of the minimap, so markers
	// keep a fixed display size at any resolution.
	enum { BLIP_UNIT, BLIP_STRUCTURE, BLIP_RESOURCE };
	struct Blip
	{
		Real fx, fy;				///< minimap fraction, row 0 at the top
		Real worldX, worldY;		///< world position, for the view cull
		Int  kind;
		Int  color;					///< house color, 0x00RRGGBB
		Bool selected;
	};

	Bool createDeviceResources();	///< Direct2D target, brush and terrain bitmap; FALSE when unavailable.
	void discardDeviceResources();
	void drawBlips(Int clientW, Int clientH);
	void drawRoads();				///< Rasterize road/bridge segments into the buffer.
	void drawThickLine(Int x0, Int y0, Int x1, Int y1, Int halfW, UnsignedInt color,
		struct RoadTex *tex = NULL, Real segLenPx = 0.0f,
		Real tintR = 1.0f, Real tintG = 1.0f, Real tintB = 1.0f);	///< Textured (or flat) thick line into the buffer.
	void drawViewBoxOverlay(Int clientW, Int clientH);	///< yellow camera-frustum box
	void drawBorderOverlay(Int clientW, Int clientH);	///< orange playable-area boundary
	Bool worldToMinimap(Real worldX, Real worldY, Int *mx, Int *my);	///< world coords -> minimap cell.
	// The coordinate mapping shared by every minimap path (terrain resample, blips,
	// roads, view box, drag-to-center). Depends on m_fullExtent: full-extent maps the
	// whole heightmap (playable + border), playable-only maps just the interior.
	//   span        = number of heightmap cells the minimap spans (per axis)
	//   originCell  = heightmap cell index at minimap pixel 0 (0 full, border playable)
	// A heightmap cell c maps to minimap pixel (c - originCell) / span * res. Returns
	// FALSE if there is no map.
	Bool mapSpans(Real *xSpan, Real *ySpan, Real *originCell);

	UnsignedInt *m_pixelBuffer;		///< terrain + roads, the source of the Direct2D bitmap.
	UnsignedInt *m_terrainBuffer;	///< cached terrain-only resample; reused when only objects change.
	Bool m_roadsValid;				///< m_pixelBuffer holds the current terrain + roads.
	Bool m_terrainValid;			///< m_terrainBuffer holds a current resample.
	Int  m_resolution;				///< current sampling/buffer edge (square).
	Bool m_terrainBuilt;
	Bool m_dragging;
	Bool m_rebuildPending;			///< A terrain change is waiting to be resampled.
	Bool m_inRebuild;				///< Re-entrancy guard: rebuildTerrain is on the stack.
	static Bool s_loading;			///< A map load/teardown is in progress (suppress rebuilds).

	Bool m_showObjects;				///< draw unit/structure dots over the terrain.
	Bool m_showRoads;				///< draw road/bridge segments over the terrain.
	Bool m_showBorder;				///< draw an orange outline at the playable-area boundary.
	Bool m_fullExtent;				///< map the full heightmap (playable + border) vs. playable area only.
	Bool m_cullObjects;				///< only draw object blips inside the 3D view frustum.
	Int  m_refreshDelayMs;			///< throttle delay; 0 = manual.

	UnsignedInt m_lastSelectionSig;	///< signature of the selection set at the last halo refresh,
									///< so notifySelectionChanged() can skip no-op clicks.

	std::vector<Blip> m_blips;

	ID2D1Factory          *m_d2dFactory;
	ID2D1HwndRenderTarget *m_d2dTarget;
	ID2D1SolidColorBrush  *m_d2dBrush;
	ID2D1Bitmap           *m_d2dBitmap;	///< m_pixelBuffer on the GPU
	Bool m_bitmapDirty;					///< m_pixelBuffer changed since the last upload
};

extern MinimapDialog *TheMinimapDialog;

#endif // __MINIMAP_DIALOG_H_
