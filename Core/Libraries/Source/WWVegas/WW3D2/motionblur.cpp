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

#include "motionblur.h"
#include "dx8caps.h"
#include "dx8wrapper.h"
#include "rendobj.h"
#include "ww3d.h"
#include <vector>

// Plenty for every aircraft on screen, and a bound on the work if something goes wrong.
static const size_t MAX_GHOST_SETS = 4096;

static std::vector<MotionBlurClass::GhostsStruct> Pool;
static size_t PoolUsed = 0;
static unsigned PoolFrame = 1;
static bool Armed = false;
static bool Debug = false;

bool MotionBlurClass::Is_Supported()
{
#if defined(BUILD_WITH_D3D9)
	if (!WW3D::Is_Sorting_Enabled() || DX8Wrapper::_Get_D3D_Device8() == nullptr)
	{
		return false;
	}
	const D3DCAPS8 & caps = DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps();
	return (caps.SrcBlendCaps & D3DPBLENDCAPS_BLENDFACTOR) != 0 && (caps.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR) != 0;
#else
	return false;
#endif
}

MotionBlurClass::GhostsStruct * MotionBlurClass::Add(RenderObjClass * mesh)
{
	if (PoolUsed >= MAX_GHOST_SETS)
	{
		return nullptr;
	}
	if (PoolUsed >= Pool.size())
	{
		Pool.resize(PoolUsed + 1);
	}
	GhostsStruct & ghosts = Pool[PoolUsed];
	mesh->Set_Motion_Blur_Slot(PoolFrame, (int)PoolUsed);
	++PoolUsed;

	ghosts.Count = 0;
	ghosts.Opacity = 0.0f;
	ghosts.Offset.Set(0.0f, 0.0f, 0.0f);
	ghosts.Spins = false;
	return &ghosts;
}

void MotionBlurClass::Set_Armed(bool armed)
{
	Armed = armed;
}

bool MotionBlurClass::Is_Armed()
{
	return Armed;
}

const MotionBlurClass::GhostsStruct * MotionBlurClass::Peek(const RenderObjClass * mesh)
{
	if (!Armed || PoolUsed == 0 || mesh->Get_Motion_Blur_Frame() != PoolFrame)
	{
		return nullptr;
	}
	const GhostsStruct & ghosts = Pool[mesh->Get_Motion_Blur_Slot()];
	return (ghosts.Count > 0) ? &ghosts : nullptr;
}

void MotionBlurClass::Set_Debug(bool debug)
{
	Debug = debug;
}

unsigned MotionBlurClass::Get_Debug_Colour(const GhostsStruct & ghosts)
{
	if (!Debug)
	{
		return 0;
	}
	return ghosts.Spins ? 0xFF00FFFF : 0xFFFF00FF;
}

void MotionBlurClass::End_Frame()
{
	PoolUsed = 0;
	++PoolFrame;
	if (PoolFrame == 0)
	{
		PoolFrame = 1;
	}
}

void MotionBlurClass::Get_Transform(const GhostsStruct & ghosts, int index, const Matrix3D & world, Matrix3D * result)
{
	if (ghosts.Spins)
	{
		Matrix3D::Multiply(world, ghosts.Spin[index], result);
	}
	else
	{
		*result = world;
	}
	const float back = -(float)(index + 1) / (float)ghosts.Count;
	result->Adjust_Translation(ghosts.Offset * back);
}

float MotionBlurClass::Get_Alpha(const GhostsStruct & ghosts, int index)
{
	return ghosts.Opacity * (1.0f - (float)index / (float)ghosts.Count);
}
