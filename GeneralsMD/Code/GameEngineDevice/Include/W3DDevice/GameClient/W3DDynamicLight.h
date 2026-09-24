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

// W3DDynamicLight.h
// Class to generate texture for terrain.
// Author: John Ahlquist, April 2001

#pragma once

#include "WW3D2/light.h"
#include "Lib/BaseType.h"
class HeightMapRenderObjClass;

/*************************************************************************
**                             W3DDynamicLight
***************************************************************************/
class W3DDynamicLight : public LightClass
{
friend class BaseHeightMapRenderObjClass;
friend class HeightMapRenderObjClass;
protected:
	/// Values used by HeightMapRenderObjClass to update the height map.
	Bool		m_priorEnable;
	Bool		m_processMe;


	Int			m_prevMinX, m_prevMinY, m_prevMaxX, m_prevMaxY;
	Int			m_minX, m_minY, m_maxX, m_maxY;

	Bool		m_enabled;
	Bool		m_terrainOnly;
	Bool		m_pixelLit;			///< the shaders draw it this frame, so the terrain's vertex lighting leaves it out
	Bool		m_bakedLastFrame;	///< the terrain's vertex lighting holds it, so it must be taken out when it leaves
	Bool		m_unitPixelLit;		///< the specular pass draws it this frame too
	const void *m_owner;

	Bool		m_decayRange;
	Bool		m_decayColor;
	UnsignedInt m_curDecayFrameCount;
	UnsignedInt m_curIncreaseFrameCount;
	UnsignedInt m_decayFrameCount;
	UnsignedInt m_increaseFrameCount;
	Real		m_targetRange;
	Vector3 m_targetAmbient;
	Vector3 m_targetDiffuse;


public:
	W3DDynamicLight();
	virtual ~W3DDynamicLight() override;

public:
	virtual void					On_Frame_Update() override;

	void setEnabled(Bool enabled) { m_enabled = enabled; m_decayRange = false; m_decayFrameCount = 0; m_decayColor = false; m_increaseFrameCount = 0; m_terrainOnly = false; m_owner = nullptr;};
	Bool isEnabled() {return m_enabled;};

	/// lights the terrain only, objects ignore it
	void setTerrainOnly(Bool terrainOnly) { m_terrainOnly = terrainOnly; }
	Bool isTerrainOnly() const { return m_terrainOnly; }

	/// set once a frame for every light, before the terrain updates its vertex lighting
	void setPixelLit(Bool pixelLit, Bool unitPixelLit) { m_pixelLit = pixelLit; m_unitPixelLit = unitPixelLit; }
	Bool isPixelLit() const { return m_pixelLit; }
	Bool isUnitPixelLit() const { return m_unitPixelLit; }

	/// whoever holds the light across frames, cleared when the pool hands it out again
	void setOwner(const void *owner) { m_owner = owner; }
	Bool isOwnedBy(const void *owner) const { return m_enabled && m_owner == owner; }


	/// 0 frameIncreaseTime means it starts out full size/intensity, 0 decay time means it lasts forever.
	void setFrameFade(UnsignedInt frameIncreaseTime, UnsignedInt decayFrameTime);
	void setDecayRange() {m_decayRange = true;};
	void setDecayColor() {m_decayColor = true;};
	// Cull returns true if the terrain vertex at x,y is outside of the light's influence.
	Bool cull(Int x, Int y ) {return (x<m_minX||y<m_minY||x>m_maxX||y>m_maxY);}
};
