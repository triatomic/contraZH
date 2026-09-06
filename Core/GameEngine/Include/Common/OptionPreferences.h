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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
// FILE: OptionPreferences.h
// Author: Matthew D. Campbell, April 2002
// Description: Saving/Loading of option preferences
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "WW3D2/ww3d.h"
#include "WW3D2/texturefilter.h"

#include "Common/UserPreferences.h"
#include "GameClient/Color.h"

typedef UnsignedInt CursorCaptureMode;
typedef UnsignedInt ScreenEdgeScrollMode;

// TheSuperHackers @feature How targeted commands (guard, attack move, abilities) are triggered.
// Only affects hotkey activation -- a mouse click on a cameo leaves the cursor over the control
// bar, where there is no world position to cast at, so it always uses the normal two step flow.
enum CastMode CPP_11(: Int)
{
	CastMode_Normal = 0,				///< click the button, then click the world (retail behavior)
	CastMode_QuickCast,					///< hotkey fires immediately at the cursor
	CastMode_QuickCastWithIndicator,	///< as above, but flash the targeting decal where it fired

	CastMode_Count,
	CastMode_Default = CastMode_Normal
};

// TheSuperHackers @feature How remaining time is shown on build queue and cooldown cameos.
// Purely a client side display preference.
enum BuildTimerDisplayMode CPP_11(: Int)
{
	BuildTimerDisplayMode_None = 0,		///< no numbers, just the existing clock sweep (retail behavior)
	BuildTimerDisplayMode_Seconds,		///< always plain seconds, however large
	BuildTimerDisplayMode_Auto,				///< seconds under a minute, M:SS above it

	BuildTimerDisplayMode_Count,
	BuildTimerDisplayMode_Default = BuildTimerDisplayMode_None
};

// TheSuperHackers @feature When health bars are shown above objects. Purely a client side
// display preference -- it never affects game logic, so it is safe in multiplayer and replays.
// TheSuperHackers @feature How big the NewRadar object blips draw. Small is the size the feature
// shipped with; Large is easier on the eye but merges sooner when units are packed together.
enum RadarBlipSize CPP_11(: Int)
{
	RadarBlipSize_Small = 0,	///< 3 pixel units, 5 pixel structures
	RadarBlipSize_Large,			///< 5 pixel units, 7 pixel structures

	RadarBlipSize_Count,
	RadarBlipSize_Default = RadarBlipSize_Large
};

enum HealthBarDisplayMode CPP_11(: Int)
{
	HealthBarDisplayMode_Classic = 0,	///< selected and moused over objects only (retail behavior)
	HealthBarDisplayMode_Damaged,			///< the above, plus anything that is not at full health
	HealthBarDisplayMode_Always,			///< the above, plus every undamaged unit and structure

	HealthBarDisplayMode_Count,
	HealthBarDisplayMode_Default = HealthBarDisplayMode_Classic
};

//-----------------------------------------------------------------------------
// OptionsPreferences options menu class
//-----------------------------------------------------------------------------
class OptionPreferences : public UserPreferences
{
public:
	OptionPreferences();
	virtual ~OptionPreferences() override;

	enum AntiAliasingMode CPP_11(: Int)
	{
		AntiAliasingMode_OFF = 0,
		AntiAliasingMode_MSAA_2X,
		AntiAliasingMode_MSAA_4X,
		AntiAliasingMode_MSAA_8X,
		AntiAliasingMode_Count
	};

	Bool loadFromIniFile();

	WW3D::MultiSampleModeEnum getAntiAliasing() const;
	TextureFilterClass::TextureFilterMode getTextureFilterMode() const;
	TextureFilterClass::AnisotropicFilterMode getTextureAnisotropyLevel() const;
	UnsignedInt getLANIPAddress();
	UnsignedInt getOnlineIPAddress();
	void setLANIPAddress(AsciiString IP);
	void setOnlineIPAddress(AsciiString IP);
	void setLANIPAddress(UnsignedInt IP);
	void setOnlineIPAddress(UnsignedInt IP);
	Bool getArchiveReplaysEnabled() const;
	Bool getAlternateMouseModeEnabled();
	Bool getRightMouseScrollWithAlternateMouseEnabled() const;
	Bool getRetaliationModeEnabled();
	HealthBarDisplayMode getHealthBarDisplayMode() const;
	BuildTimerDisplayMode getBuildTimerDisplayMode() const;
	CastMode getCastMode() const;
	Bool getSelectionCircleEnabled() const;
	Bool getObjectDecalsEnabled() const;
	Bool getEasyMilitaryDragEnabled() const;
	Bool getSmartPipsEnabled() const;
	Bool getNumericalHealthEnabled() const;
	Bool getNewRadarEnabled() const;
	RadarBlipSize getRadarBlipSize() const;
	// TheSuperHackers @feature How long a particle name lingers in the debug overlay after its
	// system is gone, in milliseconds. 0 (and an absent key) keeps the original behaviour, where a
	// name shows only while its system is alive.
	Int getParticleNameLingerMS() const;
	Bool getGridHotkeysEnabled() const;
	AsciiString getGridHotkeyLayout() const;
	Int getGridHotkeyColumns() const;
	AsciiString getNonGridHotkeys() const;
	Bool isNonGridHotkey(const AsciiString& key) const;
	// TheSuperHackers @feature Exposed statically so GlobalData can test its cached copy of the
	// list without re-reading Options.ini on every command bar rebuild.
	static Bool isNonGridHotkeyInList(const AsciiString& list, const AsciiString& key);
	Bool getKeyboardOverlayEnabled() const;
	Color getKeyboardOverlayColor() const;
	Bool getKeyboardOverlayBackdropEnabled() const;
	Color getKeyboardOverlayBackdropColor() const;
	Bool getDoubleClickAttackMoveEnabled();
	Int getJpegQuality() const;
	Real getScrollFactor();
	Bool getDrawScrollAnchor();
	Bool getMoveScrollAnchor();
	Bool getCursorCaptureEnabledInWindowedGame() const;
	Bool getCursorCaptureEnabledInWindowedMenu() const;
	Bool getCursorCaptureEnabledInFullscreenGame() const;
	Bool getCursorCaptureEnabledInFullscreenMenu() const;
	CursorCaptureMode getCursorCaptureMode() const;
	Bool getScreenEdgeScrollEnabledInWindowedApp() const;
	Bool getScreenEdgeScrollEnabledInFullscreenApp() const;
	ScreenEdgeScrollMode getScreenEdgeScrollMode() const;
	Int getFirewallBehavior();
	Short getFirewallPortAllocationDelta();
	UnsignedShort getFirewallPortOverride();
	Bool getFirewallNeedToRefresh();
	Bool usesSystemMapDir();
	AsciiString getPreferred3DProvider();
	AsciiString getSpeakerType();
	Real getSoundVolume();
	Real get3DSoundVolume();
	Real getSpeechVolume();
	Real getMusicVolume();
	Real getMoneyTransactionVolume() const;
	Bool saveCameraInReplays();
	Bool useCameraInReplays();
	Bool getPlayerObserverEnabled() const;
	Int getStaticGameDetail();
	Int getIdealStaticGameDetail();
	Real getGammaValue();
	Int getTextureReduction();
	void getResolution(Int *xres, Int *yres);
	Bool get3DShadowsEnabled();
	Bool get2DShadowsEnabled();
	Bool getCloudShadowsEnabled();
	Bool getLightmapEnabled();
	Bool getSmoothWaterEnabled();
	Bool getTreesEnabled();
	Bool getExtraAnimationsDisabled();
	Bool getUseHeatEffects();
	Bool getDynamicLODEnabled();
	Bool getFPSLimitEnabled();
	Bool getBuildingOcclusionEnabled();
	Int getParticleCap();

	Int getCampaignDifficulty();
	void setCampaignDifficulty(Int diff);

	Int getNetworkLatencyFontSize();
	Int getRenderFpsFontSize();
	Int getSystemTimeFontSize();
	Int getGameTimeFontSize();
	Int getPlayerInfoListFontSize();

	Real getResolutionFontAdjustment();

	Bool getShowMoneyPerMinute() const;

	Bool getSmartSelectionEnabled() const;

	Real getGameWindowTransitionSpeedMultiplier() const;

private:
	// TheSuperHackers @feature Read one 0-255 colour channel, clamped, with a fallback.
	UnsignedByte getColorChannel(const char *keyName, UnsignedByte defaultValue) const;
};
