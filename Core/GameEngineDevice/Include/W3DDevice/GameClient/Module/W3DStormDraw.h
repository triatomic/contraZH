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

// FILE: W3DStormDraw.h ///////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/DrawModule.h"
#include "GameClient/StormShader.h"

//-------------------------------------------------------------------------------------------------
class W3DStormDrawModuleData : public ModuleData
{
public:

	StormShaderInfo m_info;

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
/** A storm that stands around its object, follows it and dies down once the object is gone */
//-------------------------------------------------------------------------------------------------
class W3DStormDraw : public DrawModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( W3DStormDraw, "W3DStormDraw" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( W3DStormDraw, W3DStormDrawModuleData )

public:

	W3DStormDraw( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void doDrawModule(const Matrix3D* transformMtx) override;
	virtual void setShadowsEnabled(Bool enable) override { }
	virtual void releaseShadows() override {};
	virtual void allocateShadows() override {};
	virtual void setFullyObscuredByShroud(Bool fullyObscured) override;
	virtual void reactToTransformChange(const Matrix3D* oldMtx, const Coord3D* oldPos, Real oldAngle) override;
	virtual void reactToGeometryChange() override { }

protected:

	Int m_storm;	///< the storm's handle in TheW3DStorms, 0 before it starts

};
