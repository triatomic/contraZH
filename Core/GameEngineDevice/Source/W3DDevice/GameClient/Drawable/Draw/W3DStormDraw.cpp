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

// FILE: W3DStormDraw.cpp /////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>

#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "W3DDevice/GameClient/Module/W3DStormDraw.h"
#include "W3DDevice/GameClient/W3DStorm.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DStormDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);
	p.add(StormShaderInfo::getFieldParse(), offsetof( W3DStormDrawModuleData, m_info ));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DStormDraw::W3DStormDraw( Thing *thing, const ModuleData* moduleData ) : DrawModule( thing, moduleData )
{
	m_storm = 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DStormDraw::~W3DStormDraw()
{
	if (m_storm != 0 && TheW3DStorms != nullptr)
	{
		TheW3DStorms->remove(m_storm);
	}
}

//-------------------------------------------------------------------------------------------------
// The drawable draws only while it is on screen, and its storm reaches far past it, so the storm
// starts where the drawable is first placed and draws on its own from then on.
//-------------------------------------------------------------------------------------------------
void W3DStormDraw::reactToTransformChange( const Matrix3D *oldMtx, const Coord3D *oldPos, Real oldAngle )
{
	if (TheW3DStorms == nullptr)
	{
		return;
	}

	const Coord3D *position = getDrawable()->getPosition();
	if (m_storm == 0)
	{
		m_storm = TheW3DStorms->add(*position, getW3DStormDrawModuleData()->m_info, 0);
	}
	else
	{
		TheW3DStorms->move(m_storm, *position);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DStormDraw::doDrawModule(const Matrix3D* transformMtx)
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DStormDraw::setFullyObscuredByShroud( Bool fullyObscured )
{
	if (m_storm != 0 && TheW3DStorms != nullptr)
	{
		TheW3DStorms->show(m_storm, !fullyObscured);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DStormDraw::crc( Xfer *xfer )
{

	// extend base class
	DrawModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DStormDraw::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DrawModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DStormDraw::loadPostProcess()
{

	// extend base class
	DrawModule::loadPostProcess();

}
