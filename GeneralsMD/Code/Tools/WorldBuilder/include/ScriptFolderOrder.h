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

// FILE: ScriptFolderOrder.h //////////////////////////////////////////////////////////////////////
// Mod script folders that sit above the stock categories in the action and condition trees.
// MFC only; the Qt dialogs reach it through WBQtCondActBridge.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string.h>

//-------------------------------------------------------------------------------------------------
/** Position of a pinned top-level folder, or -1 for a stock folder that sorts by name. */
//-------------------------------------------------------------------------------------------------
inline int scriptFolderPinRank(const char *name)
{
	static const char *const pinned[] = { "Contra", "CLP_AI" };
	for (int i = 0; i < (int)(sizeof(pinned) / sizeof(pinned[0])); ++i)
	{
		if (strcmp(name, pinned[i]) == 0)
		{
			return i;
		}
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
/** Where a new top-level folder goes: pinned folders first in rank order, then the rest by name. */
//-------------------------------------------------------------------------------------------------
inline HTREEITEM scriptFolderInsertAfter(CTreeCtrl *tree, const char *label)
{
	const int rank = scriptFolderPinRank(label);
	HTREEITEM after = TVI_FIRST;
	for (HTREEITEM child = tree->GetRootItem(); child != nullptr; child = tree->GetNextSiblingItem(child))
	{
		CString text = tree->GetItemText(child);
		const int childRank = scriptFolderPinRank(text);
		Bool before;
		if (rank >= 0)
		{
			before = childRank >= 0 && childRank < rank;
		}
		else
		{
			before = childRank >= 0 || _stricmp(text, label) < 0;
		}
		if (!before)
		{
			break;
		}
		after = child;
	}
	return after;
}
