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

#include "StdAfx.h"
#include "resource.h"
#include "MinimapDialog.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "MainFrm.h"
#include "wbview3d.h"
#include "Common/MapObject.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingSort.h"
#include "Common/KindOf.h"
#include "Common/WellKnownKeys.h"
#include "Common/PlayerTemplate.h"
#include "Common/GlobalData.h"
#include "GameLogic/SidesList.h"
#include "GameClient/TerrainRoads.h"
#include "GameClient/Water.h"		// TheWaterTransparency (map.ini-overridable radar water color)
#include "Common/FileSystem.h"
#include "Common/MapReaderWriterInfo.h"
#include "W3DDevice/GameClient/TileData.h"
#include "WW3D2/ddsfile.h"
#include "WW3D2/ww3dformat.h"
#include "WWLib/WBParallel.h"		// parallel fork-join pool for the terrain resample
#include "WBPerf.h"
#include "GameLogic/PolygonTrigger.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include <algorithm>
#include <float.h>
#include <map>
#include <vector>
#include <d2d1.h>
#include <d2d1helper.h>

Bool localIsUnderwater(Real x, Real y);
static void clearRoadTexCache();
static void getDayNightTint(Real *tintR, Real *tintG, Real *tintB);
Bool localHasWaterAreas(void);

MinimapDialog *TheMinimapDialog = NULL;

// Coalesce a burst of terrain edits into one rebuild, like the game radar's
// throttled refresh, so painting stays smooth instead of resampling on every
// height change.
static const UINT_PTR MINIMAP_REBUILD_TIMER = 0xB01;	// arbitrary timer id

// Display size of the dialog client (the buffer is stretched to fill it).
static const int MINIMAP_DISPLAY_SIZE = 256;

// Cap on the resolution we actually RESAMPLE the terrain at (the expensive O(n^2)
// getTerrainColorAt loop). The buffer / display can be larger (e.g. the 2048
// Resolution setting) -- we resample at this cap and nearest-upsample into the full
// buffer. Since the dialog client is only ~256px, sampling above this is invisible
// but blocks the single UI thread long enough to stall the 3D viewport. 512 = 2x
// the display, plenty of detail.
static const int MINIMAP_RESAMPLE_CAP = 512;

Bool MinimapDialog::s_loading = false;

BEGIN_MESSAGE_MAP(MinimapDialog, CDialog)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_EXITSIZEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_TIMER()
END_MESSAGE_MAP()

MinimapDialog::MinimapDialog(CWnd *pParent)
	: CDialog(MinimapDialog::IDD, pParent),
	  m_pixelBuffer(NULL),
	  m_terrainBuffer(NULL),
	  m_roadsValid(false),
	  m_terrainValid(false),
	  m_resolution(MINIMAP_RES_DEFAULT),
	  m_terrainBuilt(false),
	  m_dragging(false),
	  m_rebuildPending(false),
	  m_inRebuild(false),
	  m_showObjects(true),
	  m_showRoads(true),
	  m_showBorder(true),
	  m_fullExtent(false),
	  m_refreshDelayMs(250),
	  m_lastSelectionSig(0),
	  m_d2dFactory(NULL),
	  m_d2dTarget(NULL),
	  m_d2dBrush(NULL),
	  m_d2dBitmap(NULL),
	  m_bitmapDirty(true)
{
	// Load persisted config (clamp to valid ranges).
	m_resolution     = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "Resolution", MINIMAP_RES_DEFAULT);
	m_showObjects    = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowObjects", 1) ? true : false;
	m_showRoads      = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowRoads", 1) ? true : false;
	m_showBorder     = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowBorder", 1) ? true : false;
	m_fullExtent     = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "FullExtent", 0) ? true : false;
	m_cullObjects    = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "CullObjects", 0) ? true : false;
	m_refreshDelayMs = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "RefreshDelayMs", 250);
	if (m_resolution < MINIMAP_RES_MIN) m_resolution = MINIMAP_RES_MIN;
	if (m_resolution > MINIMAP_RES_MAX) m_resolution = MINIMAP_RES_MAX;
	if (m_refreshDelayMs < 0) m_refreshDelayMs = 0;

	allocBuffer();
}

MinimapDialog::~MinimapDialog()
{
	if (TheMinimapDialog == this)
		TheMinimapDialog = NULL;
	delete [] m_pixelBuffer;
	delete [] m_terrainBuffer;
	discardDeviceResources();
	if (m_d2dFactory)
		m_d2dFactory->Release();
	clearRoadTexCache();
}

void MinimapDialog::allocBuffer()
{
	delete [] m_pixelBuffer;
	delete [] m_terrainBuffer;
	Int n = m_resolution * m_resolution;
	m_pixelBuffer   = new UnsignedInt[n];
	m_terrainBuffer = new UnsignedInt[n];
	memset(m_pixelBuffer,   0, sizeof(UnsignedInt) * n);
	memset(m_terrainBuffer, 0, sizeof(UnsignedInt) * n);
	m_terrainBuilt = false;
	m_terrainValid = false;
	m_roadsValid   = false;

	// The bitmap is sized to the buffer.
	if (m_d2dBitmap)
	{
		m_d2dBitmap->Release();
		m_d2dBitmap = NULL;
	}
	m_bitmapDirty = true;
}

BOOL MinimapDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	TheMinimapDialog = this;

	// Show the terrain buffer in a comfortably sized square client area; OnPaint
	// stretches the buffer to fit, so the display size is independent of resolution.
	CRect clientRect;
	GetClientRect(&clientRect);
	if (clientRect.Width() != MINIMAP_DISPLAY_SIZE || clientRect.Height() != MINIMAP_DISPLAY_SIZE)
	{
		CRect winRect;
		GetWindowRect(&winRect);
		int borderW = winRect.Width() - clientRect.Width();
		int borderH = winRect.Height() - clientRect.Height();
		SetWindowPos(NULL, 0, 0, MINIMAP_DISPLAY_SIZE + borderW, MINIMAP_DISPLAY_SIZE + borderH,
			SWP_NOMOVE | SWP_NOZORDER);
	}

	// Note: the saved window position is restored by the owner (CMainFrame) right after
	// Create() returns -- not here -- so there is a single place that reads Top/Left.

	return TRUE;
}

void MinimapDialog::setShowObjects(Bool show)
{
	m_showObjects = show;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "ShowObjects", show ? 1 : 0);
	if (IsWindowVisible())
	{
		// Toggling objects doesn't change terrain; reuse the cached resample if we have one.
		if (m_terrainValid)
			refreshObjects();
		else
			rebuildTerrain();
	}
}

void MinimapDialog::setShowRoads(Bool show)
{
	m_showRoads = show;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "ShowRoads", show ? 1 : 0);
	m_roadsValid = false;		// road visibility changed; rebuild the terrain+roads cache
	if (IsWindowVisible())
	{
		// Roads are composited over the cached terrain; reuse the resample if valid.
		if (m_terrainValid)
			refreshObjects();
		else
			rebuildTerrain();
	}
}

void MinimapDialog::setShowBorder(Bool show)
{
	m_showBorder = show;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "ShowBorder", show ? 1 : 0);
	// The border is drawn in OnPaint, not baked into the buffer, so a repaint is enough.
	if (::IsWindow(m_hWnd) && IsWindowVisible())
		Invalidate(FALSE);
}

void MinimapDialog::setFullExtent(Bool full)
{
	m_fullExtent = full;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "FullExtent", full ? 1 : 0);
	// Changing the extent changes the whole coordinate mapping (terrain, roads, blips,
	// view box), so the cached terrain resample no longer matches -- force a full rebuild.
	if (::IsWindow(m_hWnd) && IsWindowVisible())
	{
		m_terrainValid = false;
		m_roadsValid   = false;
		rebuildTerrain();
		Invalidate(FALSE);
	}
}

// See the header: the single coordinate mapping shared by every minimap path. span is
// the number of heightmap cells the minimap spans per axis; originCell is the cell at
// pixel 0; borderAdd shifts a border-relative coord into [0,span).
Bool MinimapDialog::mapSpans(Real *xSpan, Real *ySpan, Real *originCell)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return FALSE;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return FALSE;

	Real border  = INT_TO_REAL(pMap->getBorderSize());
	Real xExtent = INT_TO_REAL(pMap->getXExtent());
	Real yExtent = INT_TO_REAL(pMap->getYExtent());

	if (m_fullExtent)
	{
		*xSpan = xExtent;					// whole heightmap incl. border
		*ySpan = yExtent;
		*originCell = 0.0f;					// pixel 0 = heightmap corner
	}
	else
	{
		*xSpan = xExtent - 2.0f * border;	// playable interior only (original behavior)
		*ySpan = yExtent - 2.0f * border;
		*originCell = border;				// pixel 0 = playable corner (cell = border)
	}
	if (*xSpan <= 0.0f || *ySpan <= 0.0f) return FALSE;
	return TRUE;
}

void MinimapDialog::setCullObjects(Bool cull)
{
	m_cullObjects = cull;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "CullObjects", cull ? 1 : 0);
	// The cull runs per paint against the current camera.
	if (::IsWindow(m_hWnd) && IsWindowVisible())
		Invalidate(FALSE);
}

void MinimapDialog::setRefreshDelayMs(Int ms)
{
	if (ms < 0) ms = 0;
	m_refreshDelayMs = ms;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "RefreshDelayMs", ms);
}

void MinimapDialog::setResolution(Int res)
{
	if (res < MINIMAP_RES_MIN) res = MINIMAP_RES_MIN;
	if (res > MINIMAP_RES_MAX) res = MINIMAP_RES_MAX;
	if (res == m_resolution)
		return;
	m_resolution = res;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "Resolution", res);
	allocBuffer();
	if (IsWindowVisible())
		rebuildTerrain();
}

void MinimapDialog::OnCancel()
{
	ShowWindow(SW_HIDE);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowMinimap", 0);
}

void MinimapDialog::OnOK()
{
}

// Persist the window position once the user finishes moving the dialog. WM_MOVE fires
// on every pixel of the drag (hundreds of INI writes); WM_EXITSIZEMOVE fires once, when
// the mouse is released, so we write Top/Left a single time per reposition.
void MinimapDialog::OnExitSizeMove()
{
	CDialog::OnExitSizeMove();
	CRect rect;
	GetWindowRect(&rect);
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "Top", rect.top);
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "Left", rect.left);
}

// Bracket a map load/teardown. While loading, suppress all minimap rebuilds (a modal
// MessageBox during OnOpenDocument pumps messages and could otherwise fire the pending
// rebuild timer against a half-swapped document). When loading ends, kick one clean
// rebuild against the now-valid new map.
void MinimapDialog::setLoading(Bool loading)
{
	s_loading = loading;
	if (!TheMinimapDialog)
		return;
	if (loading)
	{
		// Cancel any pending throttled rebuild from the outgoing map.
		if (::IsWindow(TheMinimapDialog->m_hWnd))
			TheMinimapDialog->KillTimer(MINIMAP_REBUILD_TIMER);
		TheMinimapDialog->m_rebuildPending = false;
		TheMinimapDialog->m_terrainValid = false;
	}
	else if (TheMinimapDialog->IsWindowVisible())
	{
		// Force a full resample for the new map (terrain, water color from any new
		// map.ini override, roads, objects) and repaint -- don't let a cached buffer
		// or the m_inRebuild guard skip it.
		TheMinimapDialog->m_terrainValid = false;
		TheMinimapDialog->m_inRebuild    = false;
		TheMinimapDialog->rebuildTerrain();
		if (::IsWindow(TheMinimapDialog->m_hWnd))
			TheMinimapDialog->Invalidate(FALSE);
	}
}

void MinimapDialog::requestRebuild(Bool terrainChanged)
{
	if (s_loading)					// a map load/teardown is in progress -- don't fight it
		return;
	if (!::IsWindow(m_hWnd) || !IsWindowVisible())
		return;

	// Object change (no terrain resample needed): recomposite the cached terrain +
	// objects. An object edit may be a road edit (roads are MapObjects), so invalidate
	// the terrain+roads cache -- only camera-only refreshes (requestViewBoxRefresh) keep
	// it, which is where the savings matter.
	//
	// COALESCE through the throttle timer instead of recompositing synchronously here.
	// Object invalidations arrive in BURSTS: band-selecting or moving many objects at
	// once funnels one invalObject -> WbView3d::invalObjectInView -> requestRebuild(false)
	// PER object. A synchronous recomposite per object -- including the expensive road
	// re-rasterization, since m_roadsValid is cleared each time -- stalled the 3D viewport
	// for the whole burst whenever the minimap was open (selecting 30+ objects locked the
	// main window for ~a second). The one-shot timer just keeps resetting, so the whole
	// burst collapses into a single recomposite once it settles. Use a one-frame (16ms)
	// tick when auto-refresh is off so a lone object edit still shows promptly.
	if (!terrainChanged && m_terrainValid)
	{
		m_roadsValid     = false;
		m_rebuildPending = true;
		SetTimer(MINIMAP_REBUILD_TIMER, (UINT)(m_refreshDelayMs > 0 ? m_refreshDelayMs : 16), NULL);
		return;
	}

	// 0 = manual: don't auto-rebuild on terrain edits (only on load/toggle).
	if (m_refreshDelayMs <= 0)
		return;

	// A terrain change invalidates the cached terrain resample; an object-only change
	// can reuse it (cheap recomposite). If any pending request is a terrain change, the
	// coalesced rebuild must do the full resample.
	if (terrainChanged)
	{
		m_terrainValid = false;
		m_roadsValid   = false;		// terrain (and possibly road geometry) changed
	}

	// (Re)start the one-shot throttle timer; the actual work happens in OnTimer once
	// edits stop arriving for m_refreshDelayMs.
	m_rebuildPending = true;
	SetTimer(MINIMAP_REBUILD_TIMER, (UINT)m_refreshDelayMs, NULL);
}

// A pure SELECTION change (the selection overlay's cyan halos must track the new
// selection). Unlike requestRebuild(false), this keeps the terrain + roads bitmap:
// changing which objects are selected never moves a road.
void MinimapDialog::requestSelectionRefresh()
{
	if (s_loading)					// a map load/teardown is in progress -- don't fight it
		return;
	if (!::IsWindow(m_hWnd) || !IsWindowVisible())
		return;
	if (!m_terrainBuilt)			// nothing shown yet -- let the normal path build it
		return;

	buildBlips();
	Invalidate(FALSE);
}

// Cheap order-sensitive signature of the current selection set (FNV-1a over the selected
// MapObject pointers). Equal iff the same objects are selected -- lets us skip refreshing
// the halos when a click didn't actually change the selection. O(n), no allocation.
static UnsignedInt computeSelectionSignature()
{
	UnsignedInt sig = 2166136261u;	// FNV-1a offset basis
	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
		if (pObj->isSelected())
			sig = (sig ^ (UnsignedInt)pObj) * 16777619u;	// Win32: MapObject* fits in 32-bit
	return sig;
}

void MinimapDialog::notifySelectionChanged()
{
	// Gate cheap-first so a hidden minimap / overlay-off never reaches the redraw path:
	//   1. minimap object must exist and its window be VISIBLE (this is the guard whose
	//      absence let a *hidden* minimap stall the 3D viewport on every select/deselect),
	//   2. the selection-overlay toggle must be on (nothing on the minimap reads the
	//      selection otherwise, so a refresh would be pixel-identical wasted work),
	//   3. only THEN walk the object list for the signature, and refresh only if the
	//      selection actually changed since the last halo update.
	if (!TheMinimapDialog)
		return;
	if (!::IsWindow(TheMinimapDialog->m_hWnd) || !TheMinimapDialog->IsWindowVisible())
		return;

	// Read the overlay toggle from the 3D view's cached member (no registry hit per click).
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (!p3View || !p3View->getShowSelectionOverlay())
		return;

	UnsignedInt sig = computeSelectionSignature();
	if (sig == TheMinimapDialog->m_lastSelectionSig)
		return;						// selection unchanged -- halos already correct
	TheMinimapDialog->m_lastSelectionSig = sig;

	TheMinimapDialog->requestSelectionRefresh();	// recomposite objects only; keep roads cache
}

// A pure CAMERA move (panning, zoom, and especially dragging the minimap itself).
// Terrain, roads and blips are all world space, and the view box and the blip cull
// are evaluated per paint, so a repaint is all a camera move needs.
void MinimapDialog::requestViewBoxRefresh()
{
	if (s_loading)
		return;
	if (!::IsWindow(m_hWnd) || !IsWindowVisible())
		return;

	Invalidate(FALSE);
}

void MinimapDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == MINIMAP_REBUILD_TIMER)
	{
		KillTimer(MINIMAP_REBUILD_TIMER);
		if (m_rebuildPending)
		{
			m_rebuildPending = false;
			// Reuse the cached terrain when only objects changed; otherwise resample.
			if (m_terrainValid)
				refreshObjects();
			else
				rebuildTerrain();
		}
		return;
	}
	CDialog::OnTimer(nIDEvent);
}

Bool MinimapDialog::createDeviceResources()
{
	if (!m_d2dFactory
		&& FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory)))
	{
		m_d2dFactory = NULL;
		return FALSE;
	}

	if (!m_d2dTarget)
	{
		CRect clientRect;
		GetClientRect(&clientRect);

		// 96 DPI keeps one DIP equal to one client pixel at any display scaling.
		D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
		props.dpiX = 96.0f;
		props.dpiY = 96.0f;
		// Present immediately; a vsync wait here would stall the UI thread on every camera move.
		D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
			m_hWnd, D2D1::SizeU(clientRect.Width(), clientRect.Height()),
			D2D1_PRESENT_OPTIONS_IMMEDIATELY);
		if (FAILED(m_d2dFactory->CreateHwndRenderTarget(props, hwndProps, &m_d2dTarget)))
		{
			m_d2dTarget = NULL;
			return FALSE;
		}
	}

	if (!m_d2dBrush
		&& FAILED(m_d2dTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_d2dBrush)))
	{
		m_d2dBrush = NULL;
		return FALSE;
	}

	if (!m_d2dBitmap)
	{
		D2D1_BITMAP_PROPERTIES bmpProps = D2D1::BitmapProperties(
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
		if (FAILED(m_d2dTarget->CreateBitmap(D2D1::SizeU(m_resolution, m_resolution),
			m_pixelBuffer, m_resolution * sizeof(UnsignedInt), bmpProps, &m_d2dBitmap)))
		{
			m_d2dBitmap = NULL;
			return FALSE;
		}
		m_bitmapDirty = false;
	}
	return TRUE;
}

void MinimapDialog::discardDeviceResources()
{
	if (m_d2dBitmap)
	{
		m_d2dBitmap->Release();
		m_d2dBitmap = NULL;
	}
	if (m_d2dBrush)
	{
		m_d2dBrush->Release();
		m_d2dBrush = NULL;
	}
	if (m_d2dTarget)
	{
		m_d2dTarget->Release();
		m_d2dTarget = NULL;
	}
	m_bitmapDirty = true;
}

void MinimapDialog::OnPaint()
{
	WBPerfEvent perf("minimap paint");
	CPaintDC dc(this);

	if (!m_terrainBuilt)
		return;

	CRect clientRect;
	GetClientRect(&clientRect);
	const Int clientW = clientRect.Width();
	const Int clientH = clientRect.Height();
	if (clientW <= 0 || clientH <= 0)
		return;

	if (!createDeviceResources())
		return;

	D2D1_SIZE_U size = m_d2dTarget->GetPixelSize();
	if ((Int)size.width != clientW || (Int)size.height != clientH)
		m_d2dTarget->Resize(D2D1::SizeU(clientW, clientH));

	if (m_bitmapDirty)
	{
		m_d2dBitmap->CopyFromMemory(NULL, m_pixelBuffer, m_resolution * sizeof(UnsignedInt));
		m_bitmapDirty = false;
	}

	m_d2dTarget->BeginDraw();
	m_d2dTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	m_d2dTarget->DrawBitmap(m_d2dBitmap, D2D1::RectF(0.0f, 0.0f, (FLOAT)clientW, (FLOAT)clientH),
		1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

	if (m_showObjects)
		drawBlips(clientW, clientH);

	// The border only means something in full-extent mode; playable-only would trace the edge.
	if (m_showBorder && m_fullExtent)
		drawBorderOverlay(clientW, clientH);

	drawViewBoxOverlay(clientW, clientH);

	if (m_d2dTarget->EndDraw() == D2DERR_RECREATE_TARGET)
	{
		discardDeviceResources();
		Invalidate(FALSE);
	}
}

BOOL MinimapDialog::OnEraseBkgnd(CDC *pDC)
{
	// OnPaint covers the whole client once the terrain exists.
	if (!m_terrainBuilt)
		return CDialog::OnEraseBkgnd(pDC);
	return TRUE;
}

void MinimapDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	if (m_d2dTarget && cx > 0 && cy > 0)
		m_d2dTarget->Resize(D2D1::SizeU(cx, cy));
	Invalidate(FALSE);
}

void MinimapDialog::centerViewAtClient(CPoint point)
{
	// The 128x128 buffer is stretched to fill the client area; map the client-space
	// point back to a minimap cell.
	CRect clientRect;
	GetClientRect(&clientRect);
	Int cw = clientRect.Width()  > 0 ? clientRect.Width()  : m_resolution;
	Int ch = clientRect.Height() > 0 ? clientRect.Height() : m_resolution;

	// Clamp to the client so dragging off the edge still tracks sensibly.
	if (point.x < 0) point.x = 0;
	if (point.x >= cw) point.x = cw - 1;
	if (point.y < 0) point.y = 0;
	if (point.y >= ch) point.y = ch - 1;

	Int cellX = (point.x * m_resolution) / cw;
	Int cellY = (point.y * m_resolution) / ch;

	// Buffer row 0 is the top (world-y = m_resolution-1), so flip to world-y.
	Real worldX, worldY;
	if (!minimapToWorld(cellX, m_resolution - 1 - cellY, &worldX, &worldY))
		return;

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
		return;

	// minimapToWorld returns the camera center in BORDER-RELATIVE heightmap cell
	// units -- the space setCenterInView/m_centerPt use: setupCamera multiplies by
	// MAP_XY_FACTOR to get the camera world position in object world.
	// (Do NOT pre-multiply by MAP_XY_FACTOR here, or the camera flies off the map
	// and the viewport shows only the gray clear color.)
	// Use the deferred variant so the 3D view renders from its own paint loop rather
	// than re-entrantly from this control bar's message handler.
	WbView3d *p3d = pDoc->Get3DView();
	if (p3d)
	{
		p3d->setCenterInViewDeferred(worldX, worldY);

		// Mid-drag, both views' WM_PAINT waits behind the mouse moves, so paint them now,
		// once per frame the FPS cap allows.
		if (m_dragging && p3d->paintNowIfDue())
			UpdateWindow();
	}
}

void MinimapDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_dragging = true;
	SetCapture();
	centerViewAtClient(point);
}

void MinimapDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragging && (nFlags & MK_LBUTTON))
		centerViewAtClient(point);
	else
		CDialog::OnMouseMove(nFlags, point);
}

void MinimapDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_dragging)
	{
		m_dragging = false;
		if (::GetCapture() == m_hWnd)
			::ReleaseCapture();
	}
	CDialog::OnLButtonUp(nFlags, point);
}

Bool MinimapDialog::minimapToWorld(Int mx, Int my, Real *worldX, Real *worldY)
{
	if (!worldX || !worldY)
		return FALSE;

	if (mx < 0) mx = 0;
	if (mx >= m_resolution) mx = m_resolution - 1;
	if (my < 0) my = 0;
	if (my >= m_resolution) my = m_resolution - 1;

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
		return FALSE;

	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap)
		return FALSE;

	// Pixel i maps to ABSOLUTE heightmap cell i * span/res + originCell; subtracting
	// the border converts to the BORDER-RELATIVE cells setCenterInView/m_centerPt
	// actually use (the camera lives in the same border-relative world as objects
	// and terrain -- verified live: the old +border cell sent the camera north-east
	// of the clicked blip by exactly the border).
	Real xSpan, ySpan, originCell;
	if (!mapSpans(&xSpan, &ySpan, &originCell))
		return FALSE;

	Int border = pMap->getBorderSize();
	*worldX = mx * xSpan / (Real)m_resolution + originCell - border;
	*worldY = my * ySpan / (Real)m_resolution + originCell - border;
	return TRUE;
}

void MinimapDialog::interpolateColorForHeight(RGBColor *color,
	Real height, Real hiZ, Real midZ, Real loZ)
{
	const Real howBright = 0.30f;	// match MapPreview / radar (bigger is brighter)
	const Real howDark   = 0.60f;	// match MapPreview / radar (bigger is darker)

	if (hiZ == midZ) hiZ = midZ + 0.1f;
	if (midZ == loZ) loZ = midZ - 0.1f;
	if (hiZ == loZ) hiZ = loZ + 0.2f;

	Real t;
	RGBColor colorTarget;

	if (height >= midZ)
	{
		t = (height - midZ) / (hiZ - midZ);
		colorTarget.red   = color->red   + (1.0f - color->red)   * howBright;
		colorTarget.green = color->green + (1.0f - color->green) * howBright;
		colorTarget.blue  = color->blue  + (1.0f - color->blue)  * howBright;
	}
	else
	{
		t = (midZ - height) / (midZ - loZ);
		colorTarget.red   = color->red   + (0.0f - color->red)   * howDark;
		colorTarget.green = color->green + (0.0f - color->green) * howDark;
		colorTarget.blue  = color->blue  + (0.0f - color->blue)  * howDark;
	}

	color->red   = color->red   + (colorTarget.red   - color->red)   * t;
	color->green = color->green + (colorTarget.green - color->green) * t;
	color->blue  = color->blue  + (colorTarget.blue  - color->blue)  * t;

	if (color->red   < 0.0f) color->red   = 0.0f;
	if (color->red   > 1.0f) color->red   = 1.0f;
	if (color->green < 0.0f) color->green = 0.0f;
	if (color->green > 1.0f) color->green = 1.0f;
	if (color->blue  < 0.0f) color->blue  = 0.0f;
	if (color->blue  > 1.0f) color->blue  = 1.0f;
}

// Fills level[y * res + x] with the water height of the first water trigger that
// contains sample (x, y), or -FLT_MAX. Samples are rounded to integer map coords the
// way localIsUnderwater rounds them, and each row is tested against the trigger's
// edges with pointInTrigger's crossing rule, so the result matches it sample for sample.
static void rasterizeWaterLevels(Real *level, Int res, const Real *mapX, const Real *mapY)
{
	for (Int i = 0; i < res * res; ++i)
		level[i] = -FLT_MAX;

	std::vector<Real> crossings;
	for (PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext())
	{
		if (!pTrig->isWaterArea() || pTrig->getNumPoints() < 1)
			continue;
		const Int numPoints = pTrig->getNumPoints();
		const Real waterZ = pTrig->getPoint(0)->z;

		Int loX = pTrig->getPoint(0)->x, hiX = loX;
		Int loY = pTrig->getPoint(0)->y, hiY = loY;
		for (Int p = 1; p < numPoints; ++p)
		{
			const ICoord3D *pt = pTrig->getPoint(p);
			if (pt->x < loX) loX = pt->x;
			if (pt->x > hiX) hiX = pt->x;
			if (pt->y < loY) loY = pt->y;
			if (pt->y > hiY) hiY = pt->y;
		}

		for (Int y = 0; y < res; ++y)
		{
			const Int py = (Int)floor(mapY[y] + 0.5f);
			if (py < loY || py > hiY)
				continue;

			crossings.clear();
			for (Int p = 0; p < numPoints; ++p)
			{
				const ICoord3D pt1 = *pTrig->getPoint(p);
				const ICoord3D pt2 = *pTrig->getPoint(p == numPoints - 1 ? 0 : p + 1);
				if (pt1.y == pt2.y)
					continue;
				if (pt1.y < py && pt2.y < py)
					continue;
				if (pt1.y >= py && pt2.y >= py)
					continue;
				const Int dy = pt2.y - pt1.y;
				const Int dx = pt2.x - pt1.x;
				crossings.push_back(pt1.x + (dx * (py - pt1.y)) / ((Real)dy));
			}
			if (crossings.empty())
				continue;
			std::sort(crossings.begin(), crossings.end());

			Real *row = level + y * res;
			for (Int x = 0; x < res; ++x)
			{
				if (row[x] > -FLT_MAX)
					continue;
				const Int px = (Int)floor(mapX[x] + 0.5f);
				if (px < loX || px > hiX)
					continue;
				// Inside when an odd number of edge crossings lie at or right of the sample.
				Int right = 0;
				for (size_t c = crossings.size(); c > 0; --c)
				{
					if (crossings[c - 1] >= px)
						++right;
					else
						break;
				}
				if (right & 1)
					row[x] = waterZ;
			}
		}
	}
}

void MinimapDialog::rebuildTerrain()
{
	WBPerfEvent perf("minimap rebuildTerrain");
	// Suppress while a map load/teardown is in progress (setLoading(false) clears the
	// flag before kicking the one intended post-load rebuild, so that call passes).
	if (s_loading)
		return;

	// Re-entrancy guard: a modal MessageBox / nested message pump can fire the rebuild
	// timer while we're already mid-rebuild. Don't recurse into a half-done resample.
	if (m_inRebuild)
		return;
	m_inRebuild = true;

	// Road textures are tied to the map; clear the cache so the next drawRoads()
	// reloads textures fresh (handles map close/reopen and resolution changes).
	clearRoadTexCache();

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
	{
		m_inRebuild = false;
		return;
	}

	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap)
	{
		m_inRebuild = false;
		return;
	}

	// Water color: use the engine's radar water color, which map.ini can override
	// (WaterTransparency block, "RadarWaterColor"). TheWaterTransparency is an
	// OVERRIDE<> smart pointer, so this resolves to the map.ini value when one is
	// loaded, else the default. parseRGBColor stores 0..1, but the built-in default
	// is the legacy raw 140/140/255 (> 1); normalize those by /255 so both forms work.
	RGBColor waterColor;
	waterColor.red = 0.55f;
	waterColor.green = 0.55f;
	waterColor.blue = 1.0f;
	if (TheWaterTransparency)
	{
		RGBColor rc = TheWaterTransparency->m_radarColor;
		if (rc.red > 1.0f || rc.green > 1.0f || rc.blue > 1.0f)
		{	// legacy raw 0..255 default
			rc.red /= 255.0f; rc.green /= 255.0f; rc.blue /= 255.0f;
		}
		waterColor = rc;
	}

	// Day/night tint: getTerrainColorAt returns the raw texture color (no lighting),
	// so apply the current time-of-day terrain lighting ourselves (the same tint roads
	// use), so the minimap darkens at night like the viewport.
	Real tintR, tintG, tintB;
	getDayNightTint(&tintR, &tintG, &tintB);

	const Int   border = pMap->getBorderSize();
	const Int   res = m_resolution;					// full buffer / display resolution

	// Resample the terrain at a capped resolution (sampleRes), then nearest-upsample
	// into the full-res buffer. This keeps the expensive O(n^2) getTerrainColorAt loop
	// bounded -- at the 2048 setting it would otherwise run ~4M times and, because the
	// whole rebuild is synchronous on the single UI thread, block (stall) the 3D
	// viewport's paint for the duration. The dialog client is only ~256px, so sampling
	// above the cap is invisible anyway.
	const Int   sampleRes = (res < MINIMAP_RESAMPLE_CAP) ? res : MINIMAP_RESAMPLE_CAP;
	// Map per m_fullExtent (see mapSpans): full-extent samples the whole heightmap so
	// terrain/objects in the border margin show up; playable-only samples just the
	// interior. Pixel i maps to heightmap cell i * span/res + originCell.
	Real xSpanCells, ySpanCells, originCell;
	if (!mapSpans(&xSpanCells, &ySpanCells, &originCell))
	{
		m_inRebuild = false;
		return;
	}
	const Real  xSample = xSpanCells / (Real)sampleRes;
	const Real  ySample = ySpanCells / (Real)sampleRes;

	// Precompute the per-index heightmap coords (world cell, border included) and
	// terrain-sample coords once per row/column. getTerrainColorAt / localIsUnderwater
	// take BORDER-RELATIVE world units (MAP_XY_FACTOR * (cell - border)), so subtract
	// the border there even though the cell index itself includes it.
	Real *cellX = new Real[sampleRes];	// heightmap index along X (for getHeight)
	Real *cellY = new Real[sampleRes];	// heightmap index along Y
	Real *mapX  = new Real[sampleRes];	// scaled sample coord along X (for getTerrainColorAt / water)
	Real *mapY  = new Real[sampleRes];	// scaled sample coord along Y
	for (Int i = 0; i < sampleRes; ++i)
	{
		cellX[i] = i * xSample + originCell;
		cellY[i] = i * ySample + originCell;
		mapX[i]  = MAP_XY_FACTOR * (cellX[i] - border);	// border-relative for terrain color
		mapY[i]  = MAP_XY_FACTOR * (cellY[i] - border);
	}

	// Height stats prepass (reuse precomputed cell coords).
	Real maxHeight = -10000.0f;
	Real minHeight = 10000.0f;
	Real avgHeight = 0.0f;
	Int count = 0;
	for (Int y = 0; y < sampleRes; ++y)
	{
		for (Int x = 0; x < sampleRes; ++x)
		{
			Real h = pMap->getHeight(cellX[x], cellY[y]);
			avgHeight += h;
			if (h > maxHeight) maxHeight = h;
			if (h < minHeight) minHeight = h;
			++count;
		}
	}
	if (count > 0) avgHeight /= count;

	// Water areas as a per-sample mask: each water trigger is scanline-rasterized once
	// with the same crossing rule and first-trigger-wins order as pointInTrigger, in
	// place of testing every trigger for every sample. A sample is under water when the
	// ground there is below the level of the first trigger that contains it.
	Real *waterLevel = NULL;
	if (localHasWaterAreas())
	{
		waterLevel = new Real[sampleRes * sampleRes];
		rasterizeWaterLevels(waterLevel, sampleRes, mapX, mapY);
	}

	// Resample into a sampleRes x sampleRes scratch buffer (top-down, world-row y ->
	// row (sampleRes-1 - y)). Single sample per pixel.
	UnsignedInt *sample = new UnsignedInt[sampleRes * sampleRes];

	// Resample across worker threads: each band owns a disjoint range of rows,
	// and every pixel writes a unique slot in `sample`, so no locking is needed.
	// Everything the body reads (heightmap, tile mips via getTerrainColorAt, the
	// polygon-trigger water scan, the precomputed coord tables, and the scalar
	// tint/water color) is read-only for the duration of this loop. Set
	// WB_PARALLEL=0 to force this fully serial for A/B validation.
	WBParallel::parallelFor(0, sampleRes, [&](int yBegin, int yEnd)
	{
		for (Int y = yBegin; y < yEnd; ++y)
		{
			for (Int x = 0; x < sampleRes; ++x)
			{
				Real z = pMap->getHeight(cellX[x], cellY[y]);

				RGBColor color;

				if (waterLevel != NULL && waterLevel[y * sampleRes + x] > -FLT_MAX &&
					TheTerrainRenderObject->getHeightMapHeight(mapX[x], mapY[y], NULL) < waterLevel[y * sampleRes + x])
				{
					color = waterColor;
					interpolateColorForHeight(&color, z,
						pMap->getMaxHeightValue(), avgHeight, pMap->getMinHeightValue());
				}
				else
				{
					pMap->getTerrainColorAt(mapX[x], mapY[y], &color);
					interpolateColorForHeight(&color, z, maxHeight, avgHeight, minHeight);
				}

				color.red   *= tintR;
				color.green *= tintG;
				color.blue  *= tintB;

				sample[(sampleRes - 1 - y) * sampleRes + x] =
					(REAL_TO_INT(color.blue * 255))       |
					(REAL_TO_INT(color.green * 255) << 8)  |
					(REAL_TO_INT(color.red * 255) << 16)   |
					(255 << 24);
			}
		}
	});

	// Upsample (nearest) the scratch buffer into the full-res cached terrain buffer.
	// Both are already top-down, so this is a straight scale with no row flip.
	if (sampleRes == res)
	{
		memcpy(m_terrainBuffer, sample, sizeof(UnsignedInt) * res * res);
	}
	else
	{
		for (Int y = 0; y < res; ++y)
		{
			Int sy = (y * sampleRes) / res;
			const UnsignedInt *srow = sample + sy * sampleRes;
			UnsignedInt *drow = m_terrainBuffer + y * res;
			for (Int x = 0; x < res; ++x)
				drow[x] = srow[(x * sampleRes) / res];
		}
	}

	delete [] sample;
	delete [] waterLevel;
	delete [] cellX;
	delete [] cellY;
	delete [] mapX;
	delete [] mapY;

	m_terrainValid = true;		// cached terrain is now current
	m_roadsValid   = false;		// fresh terrain (and tint) -> rebuild the terrain+roads cache

	// Composite terrain + objects into the displayed buffer.
	refreshObjects();
	m_inRebuild = false;
}

// Cheap path: rebuild roads over the cached terrain when they changed, and refresh
// the blip list, without resampling the terrain.
void MinimapDialog::refreshObjects()
{
	WBPerfEvent perf("minimap refreshObjects");

	// Roads are camera-invariant and expensive to rasterize, so they stay baked into the
	// buffer until terrain or roads change.
	if (!m_roadsValid)
	{
		memcpy(m_pixelBuffer, m_terrainBuffer, sizeof(UnsignedInt) * m_resolution * m_resolution);
		if (m_showRoads)
			drawRoads();
		m_roadsValid = true;
		m_bitmapDirty = true;
	}

	buildBlips();

	m_terrainBuilt = true;
	if (IsWindow(m_hWnd))
		Invalidate(FALSE);
}

// Resolve a MapObject's house color exactly the way the 3D view does (and the game
// radar effectively does): originalOwner -> team -> side -> explicit playerColor
// override, else the faction PlayerTemplate's preferred color. Returns 0x00RRGGBB.
// 0xFFFFFF (white) for neutral / unowned.
static Int getHouseColorForOwner(const AsciiString &owner)
{
	Int playerColor = 0xFFFFFF;
	TeamsInfo *teamInfo = TheSidesList->findTeamInfo(owner);
	if (!teamInfo)
		return playerColor;

	AsciiString teamOwner = teamInfo->getDict()->getAsciiString(TheKey_teamOwner);
	SidesInfo *pSide = TheSidesList->findSideInfo(teamOwner);
	if (!pSide)
		return playerColor;

	Bool hasColor = false;
	Int color = pSide->getDict()->getInt(TheKey_playerColor, &hasColor);
	if (hasColor)
		return color;

	AsciiString tmplname = pSide->getDict()->getAsciiString(TheKey_playerFaction);
	const PlayerTemplate *pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
	if (pt)
		playerColor = pt->getPreferredColor()->getAsInt();
	return playerColor;
}

// Objects share a handful of owners, so each owner resolves once per blip pass.
static Int getMapObjectHouseColor(MapObject *pObj, std::map<AsciiString, Int> &ownerColors)
{
	Bool exists = false;
	AsciiString owner = pObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
	if (!exists)
		return 0xFFFFFF;
	std::map<AsciiString, Int>::iterator it = ownerColors.find(owner);
	if (it == ownerColors.end())
		it = ownerColors.insert(std::make_pair(owner, getHouseColorForOwner(owner))).first;
	return it->second;
}

// 0x00RRGGBB -> the buffer's BGRA word (opaque).
static inline UnsignedInt packBGRA(Int rgb)
{
	Int r = (rgb >> 16) & 0xFF;
	Int g = (rgb >> 8)  & 0xFF;
	Int b =  rgb        & 0xFF;
	return (UnsignedInt)(b | (g << 8) | (r << 16) | (255 << 24));
}

// Resource structures (supply docks + oil derricks) get a distinct minimap marker:
// a gold/black checkerboard with a dark-gray outline, so they stand out from the
// house-colored buildings and don't blend into any single terrain tone.
static Bool isResourceStructure(const ThingTemplate *t)
{
	if (!t)
		return FALSE;
	if (t->isKindOf(KINDOF_SUPPLY_SOURCE))
		return TRUE;
	return t->getName() == "TechOilDerrick";
}

// Map a world position to a minimap buffer cell (top-down, row already flipped).
// Always writes a cell CLAMPED to the buffer; returns FALSE if the point was off-map
// (caller may skip object dots) or TRUE if in-range. Shared by dots and the view box.
Bool MinimapDialog::worldToMinimap(Real worldX, Real worldY, Int *mx, Int *my)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return FALSE;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return FALSE;

	Int border = pMap->getBorderSize();
	Real xSpan, ySpan, originCell;
	if (!mapSpans(&xSpan, &ySpan, &originCell))
		return FALSE;

	// MapObject::getLocation() (and the road points) are in BORDER-RELATIVE world
	// units: getCoordFromCellIndex defines worldX = (cell - border) * MAP_XY_FACTOR,
	// so worldX / MAP_XY_FACTOR + border is the ABSOLUTE heightmap cell index. Pixel =
	// (cell - originCell) / span * res, so in full-extent mode (originCell 0) border-
	// margin objects map in-range, and in playable-only mode (originCell border) it
	// collapses to the original (worldX/MAP_XY_FACTOR)/playableSpan mapping. The view
	// box uses this SAME transform (getViewFrustumGroundCorners returns border-relative
	// corners).
	Real cellX = worldX / MAP_XY_FACTOR + border;
	Real cellY = worldY / MAP_XY_FACTOR + border;
	Int x = REAL_TO_INT(((cellX - originCell) / xSpan) * m_resolution);
	Int y = REAL_TO_INT(((cellY - originCell) / ySpan) * m_resolution);
	Bool inRange = (x >= 0 && x < m_resolution && y >= 0 && y < m_resolution);

	if (x < 0) x = 0;  if (x >= m_resolution) x = m_resolution - 1;
	if (y < 0) y = 0;  if (y >= m_resolution) y = m_resolution - 1;

	*mx = x;
	*my = m_resolution - 1 - y;		// flip to top-down buffer row
	return inRange;
}

// Is (x, y) inside the convex quad? True when the point is on the same side of every
// edge; the epsilon keeps points on the boundary inside.
static Bool isInQuad(const Coord3D *quad, Real x, Real y)
{
	Int positive = 0, negative = 0;
	for (Int i = 0; i < 4; ++i)
	{
		const Coord3D &a = quad[i];
		const Coord3D &b = quad[(i + 1) & 3];
		Real cross = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
		if (cross >  0.001f) ++positive;
		if (cross < -0.001f) ++negative;
	}
	return (positive == 0 || negative == 0);
}

// Draw the 3D view's camera frustum as a box, like the game radar's view box. Corners
// come from WbView3d::getViewFrustumGroundCorners (ground-plane projection of the 4
// viewport corners).
void MinimapDialog::drawViewBoxOverlay(Int clientW, Int clientH)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return;
	WbView3d *p3d = pDoc->Get3DView();
	if (!p3d) return;

	Coord3D corners[4];
	if (!p3d->getViewFrustumGroundCorners(corners))
		return;

	Int border = pMap->getBorderSize();
	Real xSpan, ySpan, originCell;
	if (!mapSpans(&xSpan, &ySpan, &originCell)) return;

	// Map each world corner to client pixels (fraction of the mapped span * client
	// size), clamped to the client so off-map corners still bound the box. Y is flipped
	// (world +y is up, client +y is down). The corners are BORDER-RELATIVE
	// (getViewFrustumGroundCorners subtracts the border), so + border gives the absolute
	// cell index; - originCell then matches worldToMinimap's transform in either extent
	// mode.
	D2D1_POINT_2F pts[4];
	for (int i = 0; i < 4; ++i)
	{
		Real fx = (corners[i].x / MAP_XY_FACTOR + border - originCell) / xSpan;
		Real fy = (corners[i].y / MAP_XY_FACTOR + border - originCell) / ySpan;
		if (fx < 0.0f) fx = 0.0f;  if (fx > 1.0f) fx = 1.0f;
		if (fy < 0.0f) fy = 0.0f;  if (fy > 1.0f) fy = 1.0f;
		pts[i] = D2D1::Point2F(fx * clientW, (1.0f - fy) * clientH);
	}

	ID2D1PathGeometry *geometry = NULL;
	if (FAILED(m_d2dFactory->CreatePathGeometry(&geometry)))
		return;
	ID2D1GeometrySink *sink = NULL;
	if (SUCCEEDED(geometry->Open(&sink)))
	{
		sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_HOLLOW);
		sink->AddLines(pts + 1, 3);
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);
		sink->Close();
		sink->Release();

		Int thickness = clientW / 128;		// ~2px at 256 client, scales with size
		if (thickness < 2) thickness = 2;
		m_d2dBrush->SetColor(D2D1::ColorF(0xFFFF00));
		m_d2dTarget->DrawGeometry(geometry, m_d2dBrush, (FLOAT)thickness);
	}
	geometry->Release();
}

// Draw the playable-area boundary as an orange rectangle, separating the playable
// region from the non-playable border margin. The playable area spans heightmap
// cells [border, extent-border].
void MinimapDialog::drawBorderOverlay(Int clientW, Int clientH)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return;

	Real border  = INT_TO_REAL(pMap->getBorderSize());
	Real xExtent = INT_TO_REAL(pMap->getXExtent());
	Real yExtent = INT_TO_REAL(pMap->getYExtent());
	if (xExtent <= 0.0f || yExtent <= 0.0f || border <= 0.0f) return;

	Real xSpan, ySpan, originCell;
	if (!mapSpans(&xSpan, &ySpan, &originCell)) return;

	Real fxL = (border           - originCell) / xSpan;
	Real fxR = (xExtent - border - originCell) / xSpan;
	Real fyB = (border           - originCell) / ySpan;		// world-space bottom edge
	Real fyT = (yExtent - border - originCell) / ySpan;		// world-space top edge

	// Y is flipped (world +y is up, client +y is down). The half-pixel inset centers the
	// 1px hairline on whole pixels.
	Int left   = (Int)(fxL * clientW);
	Int right  = (Int)(fxR * clientW);
	Int top    = (Int)((1.0f - fyT) * clientH);
	Int bottom = (Int)((1.0f - fyB) * clientH);

	m_d2dBrush->SetColor(D2D1::ColorF(0xFF8C00));
	m_d2dTarget->DrawRectangle(D2D1::RectF(left + 0.5f, top + 0.5f, right - 0.5f, bottom - 0.5f),
		m_d2dBrush, 1.0f);
}

// ----------------------------------------------------------------------------
// Road texture cache: loads each road TGA from Art/Terrain/ once per map at its
// NATIVE dimensions into a simple RGBA buffer (not the 64-tile grid that
// WorldHeightMap::readTiles assumes), for per-pixel sampling in drawThickLine.
// Cleared in rebuildTerrain() (map change) and in the destructor.

struct RoadTex
{
	UnsignedByte *rgba;		// w*h*4, RGBA, top-down. NULL = load failed.
	Int           w, h;
	UnsignedInt   avg;		// average color of the whole texture, packed 0x00RRGGBB
							// (DIB byte order: B | G<<8 | R<<16). Used to draw roads as a
							// single flat color so per-pixel asphalt detail doesn't read
							// as noise at minimap scale.
};

struct RoadTexEntry
{
	AsciiString name;
	RoadTex     tex;
};

static RoadTexEntry s_roadTexCache[64];
static Int          s_roadTexCount = 0;

// Load a road texture by its (.tga) name into a native-size RGBA buffer.
//
// Road textures are NOT loose files under Art/Terrain/ -- they live inside the
// game's .big archives and ship as DXT-compressed .dds (the engine references the
// .tga name but loads the .dds equivalent). So we reuse the engine's own
// DDSFileClass: it swaps the .tga extension to .dds, resolves the file through the
// file factory (which sees the BIG archives), and decodes the DXT block compression.
// Get_Pixel returns 0xAARRGGBB; we store it top-down as RGBA. Returns false on
// failure (caller falls back to a flat color).
static Bool loadRoadTexture(const AsciiString &texName, RoadTex *out)
{
	out->rgba = NULL; out->w = out->h = 0; out->avg = 0;
	if (texName.isEmpty())
		return FALSE;

	// DDSFileClass rewrites the last 3 chars to "dds", so the name must end in a
	// 3-char extension (the road type names always carry ".tga").
	DDSFileClass dds(texName.str(), 0);
	if (!dds.Is_Available() || !dds.Load())
		return FALSE;

	Int w = (Int)dds.Get_Width(0);
	Int h = (Int)dds.Get_Height(0);
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
		return FALSE;

	UnsignedByte *rgba = new UnsignedByte[w * h * 4];
	UnsignedInt sumR = 0, sumG = 0, sumB = 0;
	for (Int y = 0; y < h; ++y) {
		for (Int x = 0; x < w; ++x) {
			unsigned argb = dds.Get_Pixel(0, x, y);	// 0xAARRGGBB
			UnsignedByte r = (UnsignedByte)((argb >> 16) & 0xff);
			UnsignedByte g = (UnsignedByte)((argb >>  8) & 0xff);
			UnsignedByte b = (UnsignedByte)((argb      ) & 0xff);
			UnsignedByte *d = rgba + (y * w + x) * 4;
			d[0] = r; d[1] = g; d[2] = b;
			d[3] = (UnsignedByte)((argb >> 24) & 0xff);	// A
			sumR += r; sumG += g; sumB += b;
		}
	}

	// Average color of the texture (flat road fill). Packed DIB order: B | G<<8 | R<<16.
	UnsignedInt n = (UnsignedInt)(w * h);
	UnsignedInt aR = sumR / n, aG = sumG / n, aB = sumB / n;
	out->avg = aB | (aG << 8) | (aR << 16);

	out->rgba = rgba; out->w = w; out->h = h;
	return TRUE;
}

static RoadTex *getRoadTex(const AsciiString &texName)
{
	for (Int i = 0; i < s_roadTexCount; ++i)
		if (s_roadTexCache[i].name == texName)
			return s_roadTexCache[i].tex.rgba ? &s_roadTexCache[i].tex : NULL;

	RoadTex tex; tex.rgba = NULL; tex.w = tex.h = 0;
	loadRoadTexture(texName, &tex);

	RoadTex *result = NULL;
	if (s_roadTexCount < 64) {
		s_roadTexCache[s_roadTexCount].name = texName;
		s_roadTexCache[s_roadTexCount].tex = tex;
		result = tex.rgba ? &s_roadTexCache[s_roadTexCount].tex : NULL;
		++s_roadTexCount;
	} else if (tex.rgba) {
		delete [] tex.rgba;	// cache full; drop
	}
	return result;
}

static void clearRoadTexCache()
{
	for (Int i = 0; i < s_roadTexCount; ++i)
		if (s_roadTexCache[i].tex.rgba) {
			delete [] s_roadTexCache[i].tex.rgba;
			s_roadTexCache[i].tex.rgba = NULL;
		}
	s_roadTexCount = 0;
}

// Alpha-blend src (0x__RRGGBB, in DIB byte order B|G<<8|R<<16) over dst by coverage
// alpha in [0,255]. Used to feather road edges into the terrain underneath.
static inline UnsignedInt blendOver(UnsignedInt dst, UnsignedInt src, UnsignedInt a)
{
	if (a >= 255) return src | 0xFF000000u;
	if (a == 0)   return dst;
	UnsignedInt ia = 255 - a;
	UnsignedInt sb = src & 0xff, sg = (src >> 8) & 0xff, sr = (src >> 16) & 0xff;
	UnsignedInt db = dst & 0xff, dg = (dst >> 8) & 0xff, dr = (dst >> 16) & 0xff;
	UnsignedInt ob = (sb * a + db * ia + 127) / 255;
	UnsignedInt og = (sg * a + dg * ia + 127) / 255;
	UnsignedInt or_ = (sr * a + dr * ia + 127) / 255;
	return ob | (og << 8) | (or_ << 16) | 0xFF000000u;
}

// Current time-of-day terrain lighting tint (ambient + sun diffuse), clamped to
// [0,1] per channel. The terrain resample applies this so the minimap darkens at
// night like the 3D view; roads apply the same tint so they track day/night too.
// Toggled with Ctrl+D (WbView3d::stepTimeOfDay changes TheGlobalData->m_timeOfDay).
static void getDayNightTint(Real *tintR, Real *tintG, Real *tintB)
{
	*tintR = *tintG = *tintB = 1.0f;
	if (!TheGlobalData)
		return;
	const GlobalData::TerrainLighting *tl =
		&TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][0];	// 0 = sun
	Real r = tl->ambient.red   + tl->diffuse.red;
	Real g = tl->ambient.green + tl->diffuse.green;
	Real b = tl->ambient.blue  + tl->diffuse.blue;
	if (r > 1.0f) r = 1.0f;  if (r < 0.0f) r = 0.0f;
	if (g > 1.0f) g = 1.0f;  if (g < 0.0f) g = 0.0f;
	if (b > 1.0f) b = 1.0f;  if (b < 0.0f) b = 0.0f;
	*tintR = r; *tintG = g; *tintB = b;
}

// Multiply a packed 0x__RRGGBB color (DIB byte order: B | G<<8 | R<<16) by a tint.
static inline UnsignedInt applyTint(UnsignedInt c, Real tintR, Real tintG, Real tintB)
{
	UnsignedInt b = c & 0xff, g = (c >> 8) & 0xff, r = (c >> 16) & 0xff;
	UnsignedInt rr = (UnsignedInt)(r * tintR + 0.5f); if (rr > 255) rr = 255;
	UnsignedInt gg = (UnsignedInt)(g * tintG + 0.5f); if (gg > 255) gg = 255;
	UnsignedInt bb = (UnsignedInt)(b * tintB + 0.5f); if (bb > 255) bb = 255;
	return bb | (gg << 8) | (rr << 16) | (c & 0xFF000000u);
}

// Rasterize a road segment as an ORIENTED QUAD rather than stamping a square per
// centerline step. For every pixel inside the segment's bounding box we project onto
// the segment axis (-> U, distance along the road, used for texture tiling) and onto
// the perpendicular (-> signed distance from the centerline). Pixels within halfW of
// the centerline are part of the road; the outer ~1px is feathered with coverage
// anti-aliasing, and the result is alpha-blended into the terrain using the texture's
// own alpha. Each covered pixel is written exactly once (no diagonal overdraw), so
// this is both straighter-edged AND cheaper than the old square-stamp loop.
//
// halfEnd extends the band slightly past each endpoint so consecutive segments meet
// without a gap at bends (a cheap miter substitute).
void MinimapDialog::drawThickLine(Int x0, Int y0, Int x1, Int y1, Int halfW, UnsignedInt color,
	RoadTex *tex, Real segLenPx, Real tintR, Real tintG, Real tintB)
{
	Real ax = (Real)(x1 - x0), ay = (Real)(y1 - y0);
	Real len = sqrtf(ax * ax + ay * ay);

	Real fHalf = (Real)halfW + 0.5f;			// half-width including the AA edge pixel
	const Real aaWidth = 1.0f;					// width (px) of the feathered edge band

	// Roads are drawn as a single FLAT color (the texture's average) rather than
	// per-pixel sampling: the asphalt cracks/detail just read as noise at minimap
	// scale. tex->avg is the precomputed average of the loaded road texture; fall
	// back to the flat fallback color if no texture loaded. Day/night tint applied.
	UnsignedInt roadC = applyTint(tex ? tex->avg : color, tintR, tintG, tintB);

	if (len < 0.5f) {
		// Degenerate (single point): draw a small filled disc so dots/joints still show.
		Int r = halfW; if (r < 1) r = 1;
		for (Int oy = -r; oy <= r; ++oy)
			for (Int ox = -r; ox <= r; ++ox) {
				Int px = x0 + ox, py = y0 + oy;
				if (px < 0 || px >= m_resolution || py < 0 || py >= m_resolution) continue;
				if (ox*ox + oy*oy > r*r) continue;
				m_pixelBuffer[py * m_resolution + px] =
					blendOver(m_pixelBuffer[py * m_resolution + px], roadC, 255);
			}
		return;
	}

	Real inv = 1.0f / len;
	Real ux = ax * inv, uy = ay * inv;			// unit axis along the road
	// Perpendicular is (-uy, ux).

	// Bounding box of the oriented quad (centerline +/- fHalf), clipped to the buffer.
	Real cxm = (x0 + x1) * 0.5f, cym = (y0 + y1) * 0.5f;
	Real halfLen = len * 0.5f;
	Real bxr = fabsf(ux) * halfLen + fabsf(uy) * fHalf;
	Real byr = fabsf(uy) * halfLen + fabsf(ux) * fHalf;
	Int minX = (Int)floor(cxm - bxr), maxX = (Int)ceil(cxm + bxr);
	Int minY = (Int)floor(cym - byr), maxY = (Int)ceil(cym + byr);
	if (minX < 0) minX = 0;  if (maxX >= m_resolution) maxX = m_resolution - 1;
	if (minY < 0) minY = 0;  if (maxY >= m_resolution) maxY = m_resolution - 1;

	for (Int py = minY; py <= maxY; ++py) {
		Real ry = (Real)py - (Real)y0;
		for (Int px = minX; px <= maxX; ++px) {
			Real rx = (Real)px - (Real)x0;

			// Project (rx,ry) onto axis (along) and perpendicular (perp).
			Real along = rx * ux + ry * uy;
			Real perp  = rx * (-uy) + ry * ux;

			// Allow the band to run the full segment length (clamp along to [0,len] for
			// the texture coordinate, but draw a touch past the ends for joint coverage).
			if (along < -0.5f || along > len + 0.5f) continue;

			Real dist = fabsf(perp);
			if (dist > fHalf) continue;

			// Edge coverage: full inside, linearly fading over the outer aaWidth pixels.
			Real cov = 1.0f;
			Real edge = fHalf - dist;			// distance inside the outer edge
			if (edge < aaWidth) cov = edge / aaWidth;
			if (cov <= 0.0f) continue;
			if (cov > 1.0f) cov = 1.0f;

			// Flat averaged road color (precomputed above). The INTERIOR is fully
			// opaque; the only feathering is the geometric edge AA (cov), which blends
			// the road's outline into the terrain.
			UnsignedInt a = (UnsignedInt)(cov * 255.0f + 0.5f);
			if (a == 0) continue;
			Int idx = py * m_resolution + px;
			m_pixelBuffer[idx] = blendOver(m_pixelBuffer[idx], roadC, a);
		}
	}
}

// Rasterize all road and bridge segments into the pixel buffer, drawn BEFORE object
// dots so roads appear underneath units/structures. Per-pixel colors come from the
// road type's TGA texture (loaded via CachedFileInputStream, cached by name), sampled
// with bilinear filtering and proper UV tiling along the road length.
void MinimapDialog::drawRoads()
{
	if (!TheTerrainRoads)
		return;

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return;

	// Span in heightmap cells per the current extent mode; only used here to size road
	// width (the actual road endpoints map through worldToMinimap). originCell unused.
	Real xSpan, ySpan, originCell;
	if (!mapSpans(&xSpan, &ySpan, &originCell)) return;


	// The buffer (m_resolution) is scaled to the dialog client, so a width measured in
	// BUFFER pixels shrinks on screen as resolution rises -- at 2048 a road becomes
	// sub-pixel after the shrink and aliases away (the bridge vanishes first). Size
	// roads by a DISPLAY-pixel target and convert to buffer pixels, so on-screen
	// thickness is constant at every resolution. Use the real client width (the dialog
	// is resizable).
	CRect clientRect;
	GetClientRect(&clientRect);
	Int clientPx = clientRect.Width() > 0 ? clientRect.Width() : MINIMAP_DISPLAY_SIZE;
	#define ROAD_DISP_TO_BUF(d) (((d) * m_resolution + clientPx / 2) / clientPx)

	// Fallback flat colors used only when no texture could be loaded.
	const UnsignedInt roadColor   = packBGRA(0x504848);
	const UnsignedInt bridgeColor = packBGRA(0x706858);

	// Same day/night tint the terrain resample uses, so roads darken at night too.
	Real tintR, tintG, tintB;
	getDayNightTint(&tintR, &tintG, &tintB);

	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
	{
		// Roads are stored as linked pairs: POINT1 -> POINT2 (adjacent in the list).
		if (!pObj->getFlag(FLAG_ROAD_POINT1) && !pObj->getFlag(FLAG_BRIDGE_POINT1))
			continue;

		Bool isBridge = pObj->getFlag(FLAG_BRIDGE_POINT1);

		MapObject *pObj2 = pObj->getNext();
		if (!pObj2)
			continue;
		if (isBridge ? !pObj2->getFlag(FLAG_BRIDGE_POINT2) : !pObj2->getFlag(FLAG_ROAD_POINT2))
			continue;

		const Coord3D *loc1 = pObj->getLocation();
		const Coord3D *loc2 = pObj2->getLocation();

		Int mx1, my1, mx2, my2;
		worldToMinimap(loc1->x, loc1->y, &mx1, &my1);
		worldToMinimap(loc2->x, loc2->y, &mx2, &my2);

		// Look up road type for width and texture.
		Real roadWidth = 16.0f;
		RoadTex *tex = NULL;
		TerrainRoadType *roadType = TheTerrainRoads->findRoadOrBridge(pObj->getName());
		if (roadType) {
			roadWidth = roadType->getRoadWidth();
			tex = getRoadTex(roadType->getTexture());
		}

		// World-unit width -> DISPLAY-pixel half-width, then -> buffer pixels. roadWidth is
		// world units; /MAP_XY_FACTOR gives cells; *(clientPx/span) gives display pixels
		// across the on-screen minimap. Sizing in display px (not buffer px) keeps the
		// on-screen thickness constant across resolutions -- the 256 look is the reference.
		Real dispPerCell = (Real)clientPx / (xSpan < ySpan ? xSpan : ySpan);
		Real halfDisp = roadWidth / MAP_XY_FACTOR * dispPerCell / 2.0f;
		// Clamp in DISPLAY px: floor at 1 (never sub-pixel after the shrink, so the bridge
		// stays visible at 2048), cap so roads stay road-like (not slabs) at any resolution.
		if (halfDisp < 1.0f) halfDisp = 1.0f;
		if (halfDisp > 4.0f) halfDisp = 4.0f;
		Int halfW = ROAD_DISP_TO_BUF(REAL_TO_INT(halfDisp));
		if (halfW < 1) halfW = 1;

		// Segment length in buffer pixels, for UV tiling.
		Real ddx = (Real)(mx2 - mx1), ddy = (Real)(my2 - my1);
		Real segLen = sqrtf(ddx * ddx + ddy * ddy);

		UnsignedInt fallback = isBridge ? bridgeColor : roadColor;
		drawThickLine(mx1, my1, mx2, my2, halfW, fallback, tex, segLen, tintR, tintG, tintB);
	}
	#undef ROAD_DISP_TO_BUF
}

// Cache one marker per unit/structure, adapting the Thrax minimap upgrade:
//   - units:      diamond in the owner's house color
//   - structures: black-outlined box with house-color fill
//   - resource structures (supply/oil): gold/black checkerboard
void MinimapDialog::buildBlips()
{
	m_blips.clear();
	if (!m_showObjects)
		return;

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return;
	Real xSpan, ySpan, originCell;
	if (!mapSpans(&xSpan, &ySpan, &originCell)) return;
	const Real border = INT_TO_REAL(pMap->getBorderSize());

	// Selection halos share the 3D view's toggle (View > Show Object Selection Overlay).
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	const Bool showSelection = p3View ? p3View->getShowSelectionOverlay() : false;

	std::map<AsciiString, Int> ownerColors;
	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
	{
		if (pObj->isWaypoint())
			continue;
		const ThingTemplate *t = pObj->getThingTemplate();
		if (!t)
			continue;

		EditorSortingType es = t->getEditorSorting();
		Bool isStructure = (es == ES_STRUCTURE);
		Bool isUnit      = (es == ES_INFANTRY || es == ES_VEHICLE);
		if (!isStructure && !isUnit)
			continue;								// skip props/trees/debris/system/audio

		// Object locations are border-relative world units; see worldToMinimap.
		const Coord3D *loc = pObj->getLocation();
		Real fx = (loc->x / MAP_XY_FACTOR + border - originCell) / xSpan;
		Real fy = (loc->y / MAP_XY_FACTOR + border - originCell) / ySpan;
		if (fx < 0.0f || fx >= 1.0f || fy < 0.0f || fy >= 1.0f)
			continue;

		Blip b;
		b.fx = fx;
		b.fy = 1.0f - fy;
		b.worldX = loc->x;
		b.worldY = loc->y;
		b.selected = showSelection && pObj->isSelected();
		if (isUnit)
			b.kind = BLIP_UNIT;
		else if (isResourceStructure(t))
			b.kind = BLIP_RESOURCE;
		else
			b.kind = BLIP_STRUCTURE;
		b.color = (b.kind == BLIP_RESOURCE) ? 0 : getMapObjectHouseColor(pObj, ownerColors);
		m_blips.push_back(b);
	}
}

// A w x w box whose top-left sits at (cx - w/2, cy - w/2), on whole pixels.
static D2D1_RECT_F blipBox(Int cx, Int cy, Int w)
{
	FLOAT left = (FLOAT)(cx - w / 2);
	FLOAT top  = (FLOAT)(cy - w / 2);
	return D2D1::RectF(left, top, left + w, top + w);
}

void MinimapDialog::drawBlips(Int clientW, Int clientH)
{
	// Marker sizes in display pixels.
	const Int unitSize     = 7;		// diamond diagonal
	const Int structSize   = 6;
	const Int resourceSize = 7;
	const Int outline      = 1;
	const Int haloPad      = 2;
	const Int checkerCell  = 2;

	// Cull against the camera at paint time, so camera moves need no blip rebuild.
	Coord3D corners[4];
	Bool cull = false;
	if (m_cullObjects)
	{
		CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
		WbView3d *p3d = pDoc ? pDoc->Get3DView() : NULL;
		cull = p3d != NULL && p3d->getViewFrustumGroundCorners(corners);
	}

	const D2D1::ColorF cyan(0x33FFFF);		// selection halo (matches 3D view)
	const D2D1::ColorF black(0x000000);
	const D2D1::ColorF gold(0xFFD700);
	const D2D1::ColorF darkGray(0x404040);

	for (size_t i = 0; i < m_blips.size(); ++i)
	{
		const Blip &b = m_blips[i];
		if (cull && !isInQuad(corners, b.worldX, b.worldY))
			continue;
		const Int cx = (Int)(b.fx * clientW);
		const Int cy = (Int)(b.fy * clientH);

		if (b.kind == BLIP_UNIT)
		{
			// A square turned 45 degrees about the pixel center draws the diamond.
			const FLOAT px = cx + 0.5f;
			const FLOAT py = cy + 0.5f;
			m_d2dTarget->SetTransform(D2D1::Matrix3x2F::Rotation(45.0f, D2D1::Point2F(px, py)));
			if (b.selected)
			{
				const FLOAT half = (unitSize + haloPad * 2) * 0.35355f;
				m_d2dBrush->SetColor(cyan);
				m_d2dTarget->FillRectangle(D2D1::RectF(px - half, py - half, px + half, py + half), m_d2dBrush);
			}
			const FLOAT half = unitSize * 0.35355f;
			m_d2dBrush->SetColor(D2D1::ColorF((UINT32)b.color));
			m_d2dTarget->FillRectangle(D2D1::RectF(px - half, py - half, px + half, py + half), m_d2dBrush);
			m_d2dTarget->SetTransform(D2D1::Matrix3x2F::Identity());
			continue;
		}

		if (b.selected)
		{
			m_d2dBrush->SetColor(cyan);
			m_d2dTarget->FillRectangle(blipBox(cx, cy, structSize + haloPad * 2), m_d2dBrush);
		}

		if (b.kind == BLIP_RESOURCE)
		{
			// Dark-gray rim around a gold/black checker, distinct from any terrain tone.
			m_d2dBrush->SetColor(darkGray);
			m_d2dTarget->FillRectangle(blipBox(cx, cy, resourceSize), m_d2dBrush);
			const Int inner = resourceSize - outline * 2;
			const D2D1_RECT_F innerRect = blipBox(cx, cy, inner);
			m_d2dBrush->SetColor(black);
			m_d2dTarget->FillRectangle(innerRect, m_d2dBrush);
			m_d2dBrush->SetColor(gold);
			for (Int y = 0; y < inner; y += checkerCell)
			{
				for (Int x = 0; x < inner; x += checkerCell)
				{
					if ((((x / checkerCell) + (y / checkerCell)) & 1) == 0)
					{
						continue;
					}
					const FLOAT l = innerRect.left + x;
					const FLOAT t = innerRect.top + y;
					const FLOAT r = (x + checkerCell < inner) ? l + checkerCell : innerRect.right;
					const FLOAT btm = (y + checkerCell < inner) ? t + checkerCell : innerRect.bottom;
					m_d2dTarget->FillRectangle(D2D1::RectF(l, t, r, btm), m_d2dBrush);
				}
			}
			continue;
		}

		m_d2dBrush->SetColor(black);
		m_d2dTarget->FillRectangle(blipBox(cx, cy, structSize), m_d2dBrush);
		m_d2dBrush->SetColor(D2D1::ColorF((UINT32)b.color));
		m_d2dTarget->FillRectangle(blipBox(cx, cy, structSize - outline * 2), m_d2dBrush);
	}
}
