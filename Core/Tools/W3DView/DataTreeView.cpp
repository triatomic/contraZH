/*
**	Command & Conquer Renegade(tm)
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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : W3DView                                                      *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/Tools/W3DView/DataTreeView.cpp               $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 6/18/01 9:11a                                               $*
 *                                                                                             *
 *                    $Revision:: 35                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "StdAfx.h"
#include <uxtheme.h>
#include <vssym32.h>
#pragma comment(lib, "uxtheme.lib")
#include "W3DView.h"
#include "DataTreeView.h"
#include "WW3D2/rendobj.h"
#include "ViewerAssetMgr.h"
#include "Globals.h"
#include "W3DViewDoc.h"
#include "MainFrm.h"
#include "WW3D2/distlod.h"
#include "WW3D2/animobj.h"
#include "WW3D2/hcanim.h"
#include "AssetInfo.h"
#include "Utils.h"
#include "WWLib/Vector.h"
#include "W3DDarkMode.h"
#include "MaterialViewer/MaterialViewerFrame.h"
#include "WW3D2/part_emt.h"
#include "WW3D2/agg_def.h"
#include "WW3D2/bmp2d.h"
#include "WW3D2/hlod.h"
#include "WW3D2/mesh.h"
#include "WW3D2/w3d_file.h"
#include "ViewerScene.h"
#include "WW3D2/texture.h"
#include "WWLib/hashtemplate.h"
#include "WWLib/wwstring.h"
#include "WW3D2/matinfo.h"
#include "WW3D2/htree.h"

#ifdef RTS_DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


////////////////////////////////////////////////////////////////////////////
//	Local Prototypes
////////////////////////////////////////////////////////////////////////////
void Set_Highest_LOD (RenderObjClass *render_obj);


////////////////////////////////////////////////////////////////////////////
//	MFC Stuff
////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CDataTreeView, CTreeView)


////////////////////////////////////////////////////////////////////////////
//
//  CDataTreeView
//
////////////////////////////////////////////////////////////////////////////
CDataTreeView::CDataTreeView ()
        : m_hMaterialsRoot (nullptr),
			 m_hMeshRoot  (nullptr),
			 m_hMeshCollectionRoot (nullptr),
			 m_hAggregateRoot (nullptr),
			 m_hPrimitivesRoot (nullptr),
			 m_hEmitterRoot (nullptr),
          m_hLODRoot (nullptr),
			 m_hSoundRoot (nullptr),
			 m_iPrimitivesIcon (-1),
          m_iAnimationIcon (-1),
          m_iTCAnimationIcon(-1),
			 m_iADAnimationIcon(-1),
          m_iMeshIcon (-1),
          m_iMaterialIcon (-1),
          m_iLODIcon (-1),
			 m_iAggregateIcon (-1),
			 m_iEmitterIcon (-1),
			 m_iSoundIcon (-1),
			 m_iBoneIcon (-1),
			 m_hRightClickItem (nullptr),
			 m_RestrictAnims (true),
			 m_hLastMultiSelectAnchor (nullptr)

{
}


////////////////////////////////////////////////////////////////////////////
//
//  ~CDataTreeView
//
CDataTreeView::~CDataTreeView ()
{
}


BEGIN_MESSAGE_MAP(CDataTreeView, CTreeView)
	//{{AFX_MSG_MAP(CDataTreeView)
	ON_WM_CREATE()
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChanged)
	ON_NOTIFY_REFLECT(TVN_DELETEITEM, OnDeleteItem)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
	ON_WM_LBUTTONDOWN()
	ON_COMMAND(IDM_TOGGLE_MESH_VIS, OnToggleMeshVis)
	ON_COMMAND(IDM_SHOW_BONES, OnShowBones)
	ON_COMMAND(IDM_EXPORT_INFO, OnExportInfo)
	ON_COMMAND(IDM_DUMP_W3D_INFO, OnDumpW3DInfo)
	ON_COMMAND(IDM_MATVIEWER_FROM_TREE, OnOpenInMaterialViewer)
	ON_MESSAGE(WM_W3DVIEW_THEME_CHANGED, OnW3DViewThemeChanged)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////
//
//  OnDraw
//
void
CDataTreeView::OnDraw (CDC *pDC)
{
}

/////////////////////////////////////////////////////////////////////////////
// CDataTreeView diagnostics

#ifdef RTS_DEBUG
void CDataTreeView::AssertValid() const
{
	CTreeView::AssertValid();
}

void CDataTreeView::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
#endif //RTS_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDataTreeView message handlers


////////////////////////////////////////////////////////////////////////////
//
//  PreCreateWindow
//
BOOL
CDataTreeView::PreCreateWindow (CREATESTRUCT& cs)
{
    // Modify the style bits for the window so it will
    // have buttons and lines between nodes.
    cs.style |= TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;

    // Allow the base class to process this message
	return CTreeView::PreCreateWindow(cs);
}

////////////////////////////////////////////////////////////////////////////
//
//  OnInitialUpdate
//
void
CDataTreeView::OnInitialUpdate ()
{
	// Allow the base class to process this message
    CTreeView::OnInitialUpdate ();

	// TODO: Add your specialized code here and/or call the base class
}


////////////////////////////////////////////////////////////////////////////
//
//  CreateRootNodes
//
void
CDataTreeView::CreateRootNodes ()
{
	// Insert all the root nodes
	m_hMaterialsRoot		= GetTreeCtrl ().InsertItem ("Materials", m_iMaterialIcon, m_iMaterialIcon);
	m_hMeshRoot				= GetTreeCtrl ().InsertItem ("Mesh", m_iMeshIcon, m_iMeshIcon);
	m_hHierarchyRoot		= GetTreeCtrl ().InsertItem ("Hierarchy", m_iHierarchyIcon, m_iHierarchyIcon);
	m_hLODRoot				= GetTreeCtrl ().InsertItem ("H-LOD", m_iLODIcon, m_iLODIcon);
	m_hMeshCollectionRoot = GetTreeCtrl ().InsertItem ("Mesh Collection", m_iMeshIcon, m_iMeshIcon);
	m_hAggregateRoot		= GetTreeCtrl ().InsertItem ("Aggregate", m_iAggregateIcon, m_iAggregateIcon);
	m_hEmitterRoot			= GetTreeCtrl ().InsertItem ("Emitter", m_iEmitterIcon, m_iEmitterIcon);
	m_hPrimitivesRoot		= GetTreeCtrl ().InsertItem ("Primitives", m_iPrimitivesIcon, m_iPrimitivesIcon);
	m_hSoundRoot			= GetTreeCtrl ().InsertItem ("Sounds", m_iSoundIcon, m_iSoundIcon);
}


////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
int
CDataTreeView::OnCreate (LPCREATESTRUCT lpCreateStruct)
{
	if (CTreeView::OnCreate(lpCreateStruct) == -1)
		return -1;

    // Build the tree-node icon list. Pulled into a helper so it can be re-run when
    // the user toggles dark mode at runtime.
    RebuildNormalImageList ();

    // TheSuperHackers @feature Tria 22/04/2026 Themed checkbox state icons for mesh visibility.
    // The actual bitmap construction is delegated to RebuildStateImageList so it can be
    // re-invoked when the user toggles dark mode at runtime.
    RebuildStateImageList ();

    // Create the root nodes that will hold the contain
    // asset types.
    CreateRootNodes ();
	return 0;
}


////////////////////////////////////////////////////////////////////////////
//
//  Load_Materials_Into_Tree
//
// TheSuperHackers @feature Tria 18/04/2026 Group materials under their source W3D file name,
// then under each render object, so that multiple imported files show cascaded material lists.
//
static void Collect_Textures_From_RenderObj (RenderObjClass *robj, HashTemplateClass<StringClass, int> &outTextures)
{
	if (robj == nullptr)
		return;

	// Try to get material info directly (works for MeshClass)
	MaterialInfoClass *matinfo = robj->Get_Material_Info ();
	if (matinfo != nullptr) {
		for (int t = 0; t < matinfo->Texture_Count (); ++t) {
			TextureClass *tex = matinfo->Peek_Texture (t);
			if (tex != nullptr) {
				StringClass name (tex->Get_Full_Path ());
				if (!outTextures.Exists (name))
					outTextures.Insert (name, 1);
			}
		}
		REF_PTR_RELEASE (matinfo);
	}

	// Recurse into sub-objects (HLod, collections, etc.)
	int numSub = robj->Get_Num_Sub_Objects ();
	for (int s = 0; s < numSub; ++s) {
		RenderObjClass *sub = robj->Get_Sub_Object (s);
		if (sub != nullptr) {
			Collect_Textures_From_RenderObj (sub, outTextures);
			sub->Release_Ref ();
		}
	}
}

void
CDataTreeView::Load_Materials_Into_Tree ()
{
	CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();

	// Build existing tree structure: file nodes and their child object nodes
	// Level 1: file name -> HTREEITEM
	HashTemplateClass<StringClass, HTREEITEM> existingFileNodes;
	// Level 2: "filename\0objname" -> HTREEITEM
	HashTemplateClass<StringClass, HTREEITEM> existingObjNodes;
	// All existing texture names (for dedup)
	HashTemplateClass<StringClass, int> allExistingTextures;

	for (HTREEITEM hFile = GetTreeCtrl().GetChildItem(m_hMaterialsRoot); hFile; hFile = GetTreeCtrl().GetNextSiblingItem(hFile)) {
		CString fileText = GetTreeCtrl().GetItemText(hFile);

		// Check if this is a file group node (has children that are object nodes) or a direct texture
		HTREEITEM hFirstChild = GetTreeCtrl().GetChildItem(hFile);
		if (hFirstChild != nullptr) {
			// This is a grouping node
			existingFileNodes.Insert(StringClass((LPCTSTR)fileText), hFile);

			for (HTREEITEM hObj = hFirstChild; hObj; hObj = GetTreeCtrl().GetNextSiblingItem(hObj)) {
				CString objText = GetTreeCtrl().GetItemText(hObj);
				StringClass compoundKey;
				compoundKey.Format("%s|%s", (LPCTSTR)fileText, (LPCTSTR)objText);
				existingObjNodes.Insert(compoundKey, hObj);

				for (HTREEITEM hTex = GetTreeCtrl().GetChildItem(hObj); hTex; hTex = GetTreeCtrl().GetNextSiblingItem(hTex)) {
					CString texText = GetTreeCtrl().GetItemText(hTex);
					allExistingTextures.Insert(StringClass((LPCTSTR)texText), 1);
				}
			}
		}
	}

	bool needsSortRoot = false;

	// Iterate all render objects and group their textures under file -> object -> textures
	RenderObjIterator *pObjEnum = WW3DAssetManager::Get_Instance()->Create_Render_Obj_Iterator ();
	if (pObjEnum != nullptr) {

		for (pObjEnum->First (); !pObjEnum->Is_Done (); pObjEnum->Next ()) {

			LPCTSTR pszObjName = pObjEnum->Current_Item_Name ();
			// TheSuperHackers @performance Tria 2026-05-19 Removed redundant Render_Obj_Exists
			// hash lookup - the iterator is walking the same prototype list it would query,
			// so the name is guaranteed present.

			// Collect textures used by this object
			HashTemplateClass<StringClass, int> objTextures;
			RenderObjClass *robj = WW3DAssetManager::Get_Instance()->Create_Render_Obj (pszObjName);
			if (robj != nullptr) {
				Collect_Textures_From_RenderObj (robj, objTextures);
				robj->Release_Ref ();
			}

			// Check if any textures were found
			HashTemplateIterator<StringClass, int> checkIte (objTextures);
			checkIte.First ();
			if (checkIte.Is_Done ())
				continue;

			// Determine the source W3D filename for this render object
			StringClass objNameKey (pszObjName);
			StringClass sourceFileName;
			if (pdoc != nullptr) {
				const HashTemplateClass<StringClass, StringClass> &sourceMap = pdoc->GetAssetSourceFileMap ();
				if (sourceMap.Exists (objNameKey))
					sourceFileName = sourceMap.Get (objNameKey);
				else
					sourceFileName = "Unknown";
			} else {
				sourceFileName = "Unknown";
			}

			// TheSuperHackers @tweak Tria 19/04/2026 Display hierarchy name (without .w3d extension)
			// as the group node label, matching the Bones tree grouping style.
			{
				int len = sourceFileName.Get_Length ();
				if (len > 4 && _stricmp ((LPCTSTR)sourceFileName + len - 4, ".w3d") == 0)
					sourceFileName.Erase (len - 4, 4);
			}

			// Find or create the file group node (level 1)
			HTREEITEM hFileNode = nullptr;
			if (existingFileNodes.Exists (sourceFileName)) {
				hFileNode = existingFileNodes.Get (sourceFileName);
			} else {
				hFileNode = GetTreeCtrl ().InsertItem ((LPCTSTR)sourceFileName, m_iHierarchyIcon, m_iHierarchyIcon, m_hMaterialsRoot, TVI_LAST);
				existingFileNodes.Insert (sourceFileName, hFileNode);
				needsSortRoot = true;
			}

			// Find or create the object node (level 2)
			StringClass compoundKey;
			compoundKey.Format ("%s|%s", (LPCTSTR)sourceFileName, pszObjName);
			HTREEITEM hObjNode = nullptr;
			if (existingObjNodes.Exists (compoundKey)) {
				hObjNode = existingObjNodes.Get (compoundKey);
			} else {
				hObjNode = GetTreeCtrl ().InsertItem (pszObjName, m_iMeshIcon, m_iMeshIcon, hFileNode, TVI_LAST);
				existingObjNodes.Insert (compoundKey, hObjNode);
			}

			// Build a set of textures already under this object node
			HashTemplateClass<StringClass, int> existingChildren;
			for (HTREEITEM h = GetTreeCtrl().GetChildItem(hObjNode); h; h = GetTreeCtrl().GetNextSiblingItem(h)) {
				CString text = GetTreeCtrl().GetItemText(h);
				existingChildren.Insert(StringClass((LPCTSTR)text), 1);
			}

			// Insert textures as children of the object node (level 3)
			bool childNeedsSort = false;
			HashTemplateIterator<StringClass, int> texIte (objTextures);
			for (texIte.First (); !texIte.Is_Done (); texIte.Next ()) {
				StringClass texName = texIte.Peek_Key ();
				if (existingChildren.Exists (texName))
					continue;

				// Look up the TextureClass from the global hash.
				// The hash is keyed by Get_Texture_Name() (.tga), but texName is from Get_Full_Path()
				// which may now be .dds. Search the hash by matching Get_Full_Path() directly.
				TextureClass *ptexture = nullptr;
				HashTemplateClass<StringClass, TextureClass*> &texHash = WW3DAssetManager::Get_Instance()->Texture_Hash();
				if (texHash.Exists (texName)) {
					ptexture = texHash.Get (texName);
				} else {
					HashTemplateIterator<StringClass, TextureClass*> findIte (texHash);
					for (findIte.First (); !findIte.Is_Done (); findIte.Next ()) {
						TextureClass *candidate = findIte.Peek_Value ();
						if (candidate != nullptr && candidate->Get_Full_Path() == texName) {
							ptexture = candidate;
							break;
						}
					}
				}

				if (ptexture == nullptr)
					continue;

				HTREEITEM hTexItem = GetTreeCtrl ().InsertItem ((LPCTSTR)texName, m_iMaterialIcon, m_iMaterialIcon, hObjNode, TVI_LAST);
				ptexture->Add_Ref ();
				AssetInfoClass *asset_info = new AssetInfoClass ((LPCTSTR)texName, TypeMaterial, nullptr, (DWORD)ptexture);
				GetTreeCtrl ().SetItemData (hTexItem, (ULONG)asset_info);

				existingChildren.Insert (texName, 1);
				allExistingTextures.Insert (texName, 1);
				childNeedsSort = true;
			}

			if (childNeedsSort)
				GetTreeCtrl ().SortChildren (hObjNode);
		}

		SAFE_DELETE (pObjEnum);
	}

	// Add any remaining textures not claimed by a render object (e.g. standalone textures)
	HashTemplateIterator<StringClass, TextureClass*> globalIte (WW3DAssetManager::Get_Instance()->Texture_Hash());
	for (globalIte.First (); !globalIte.Is_Done (); globalIte.Next ()) {
		TextureClass *ptexture = globalIte.Peek_Value ();
		if (ptexture == nullptr)
			continue;

		StringClass texName (ptexture->Get_Full_Path ());
		if (allExistingTextures.Exists (texName))
			continue;

		HTREEITEM hTexItem = GetTreeCtrl ().InsertItem ((LPCTSTR)texName, m_iMaterialIcon, m_iMaterialIcon, m_hMaterialsRoot, TVI_LAST);
		ptexture->Add_Ref ();
		AssetInfoClass *asset_info = new AssetInfoClass ((LPCTSTR)texName, TypeMaterial, nullptr, (DWORD)ptexture);
		GetTreeCtrl ().SetItemData (hTexItem, (ULONG)asset_info);

		allExistingTextures.Insert (texName, 1);
		needsSortRoot = true;
	}

	if (needsSortRoot)
		GetTreeCtrl ().SortChildren (m_hMaterialsRoot);

}


////////////////////////////////////////////////////////////////////////////
//
//  Load_Bones_Into_Tree
//
// TheSuperHackers @feature Tria 22/04/2026 Populate Sub-Objects and Bones sub-folders inside each
// Hierarchy node. Sub-objects with normal mesh geometry go into Sub-Objects (with visibility checkbox);
// all others (no geometry, collision-only, etc.) go into Bones.
//
void
CDataTreeView::Load_Bones_Into_Tree (void)
{
	// Build a map from hierarchy name -> its tree node under m_hHierarchyRoot
	HashTemplateClass<StringClass, HTREEITEM> hierNodeMap;
	for (HTREEITEM h = GetTreeCtrl ().GetChildItem (m_hHierarchyRoot); h; h = GetTreeCtrl ().GetNextSiblingItem (h)) {
		AssetInfoClass *info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (h);
		if (info != nullptr)
			hierNodeMap.Insert (StringClass (info->Get_Name ()), h);
	}

	// Track which hierarchy names we have already populated to avoid re-doing them
	HashTemplateClass<StringClass, int> processedHierarchies;

	RenderObjIterator *pObjEnum = WW3DAssetManager::Get_Instance ()->Create_Render_Obj_Iterator ();
	if (pObjEnum == nullptr)
		return;

	for (pObjEnum->First (); !pObjEnum->Is_Done (); pObjEnum->Next ()) {

		LPCTSTR pszItemName = pObjEnum->Current_Item_Name ();
		// TheSuperHackers @performance Tria 2026-05-19 Removed redundant Render_Obj_Exists
		// hash lookup - iterator already walks the same list.

		// Only HLod / DistLOD objects carry sub-objects
		int classId = pObjEnum->Current_Item_Class_ID ();
		if (classId != RenderObjClass::CLASSID_HLOD && classId != RenderObjClass::CLASSID_DISTLOD)
			continue;

		RenderObjClass *robj = WW3DAssetManager::Get_Instance ()->Create_Render_Obj (pszItemName);
		if (robj == nullptr)
			continue;

		StringClass objName (pszItemName);

		// Skip if already processed
		if (processedHierarchies.Exists (objName)) {
			robj->Release_Ref ();
			continue;
		}
		processedHierarchies.Insert (objName, 1);

		// Find this object's node in the Hierarchy tree
		if (!hierNodeMap.Exists (objName)) {
			robj->Release_Ref ();
			continue;
		}
		HTREEITEM hHierNode = hierNodeMap.Get (objName);

		// Find or create the Sub-Objects and Bones sub-folders
		HTREEITEM hObjectsFolder = nullptr;
		HTREEITEM hBonesFolder   = nullptr;
		for (HTREEITEM hChild = GetTreeCtrl ().GetChildItem (hHierNode); hChild; hChild = GetTreeCtrl ().GetNextSiblingItem (hChild)) {
			CString text = GetTreeCtrl ().GetItemText (hChild);
			if (text == "Sub-Objects") hObjectsFolder = hChild;
			else if (text == "Bones") hBonesFolder = hChild;
		}
		if (hObjectsFolder == nullptr) {
			hObjectsFolder = GetTreeCtrl ().InsertItem ("Sub-Objects", m_iMeshIcon, m_iMeshIcon, hHierNode, TVI_FIRST);
			GetTreeCtrl ().SetItemState (hObjectsFolder, INDEXTOSTATEIMAGEMASK (2), TVIS_STATEIMAGEMASK);
		}
		if (hBonesFolder == nullptr)
			hBonesFolder = GetTreeCtrl ().InsertItem ("Bones", m_iBoneIcon, m_iBoneIcon, hHierNode, TVI_LAST);

		// Build a short-name->sub-object map for quick lookup.
		// Sub-object names are "HIERARCHY.SUBOBJ" format; pivot names are just "SUBOBJ".
		// Strip the hierarchy prefix when keying the map.
		int numSubs = robj->Get_Num_Sub_Objects ();
		HashTemplateClass<StringClass, RenderObjClass *> subObjMap;
		for (int i = 0; i < numSubs; ++i) {
			RenderObjClass *sub = robj->Get_Sub_Object (i);
			if (sub == nullptr)
				continue;
			const char *fullName = sub->Get_Name ();
			const char *dot = ::strchr (fullName, '.');
			StringClass shortName (dot != nullptr ? dot + 1 : fullName);
			subObjMap.Insert (shortName, sub);
		}

		// Iterate HTree pivots (skip pivot 0 = root).
		// Bones and sub-objects are mixed together in the Sub-Objects folder, nested by pivot
		// parent-child relationship. Each pivot is inserted alphabetically under its parent.
		// Pure bones whose parent is the root (no geometry ancestry) fall into hBonesFolder.
		const HTreeClass *htree = robj->Get_HTree ();
		int numPivots = (htree != nullptr) ? htree->Num_Pivots () : 0;

		// Map from pivot index -> HTREEITEM (for both sub-objects and bones placed in sub-objects folder).
		DynamicVectorClass<HTREEITEM> pivotItems;
		pivotItems.Resize (numPivots);
		for (int i = 0; i < numPivots; ++i)
			pivotItems[i] = nullptr;

		for (int p = 1; p < numPivots; ++p) {
			const char *pivotName = htree->Get_Bone_Name (p);
			if (pivotName == nullptr || pivotName[0] == '\0')
				continue;

			bool hasMesh = subObjMap.Exists (StringClass (pivotName));
			int parentIdx = htree->Get_Parent_Index (p);

			// Determine where to insert: under parent's tree item if it exists, otherwise root folder.
			// If parent is pivot 0 (hierarchy root) or has no tree item, use hObjectsFolder.
			// Pure bones whose ancestry never reaches a sub-object go to hBonesFolder instead.
			HTREEITEM hParentItem = nullptr;
			if (parentIdx > 0 && pivotItems[parentIdx] != nullptr) {
				hParentItem = pivotItems[parentIdx]; // nested under a sibling sub-object or bone
			} else if (parentIdx == 0) {
				hParentItem = hasMesh ? hObjectsFolder : hBonesFolder;
			} else {
				// Parent pivot exists but has no tree item (shouldn't happen in well-formed data).
				hParentItem = hasMesh ? hObjectsFolder : hBonesFolder;
			}

			if (hasMesh) {
				HTREEITEM hItem = GetTreeCtrl ().InsertItem (pivotName, m_iMeshIcon, m_iMeshIcon, hParentItem, TVI_SORT);
				AssetInfoClass *asset_info = new AssetInfoClass (pivotName, TypeMesh);
				GetTreeCtrl ().SetItemData (hItem, (ULONG)asset_info);
				GetTreeCtrl ().SetItemState (hItem, INDEXTOSTATEIMAGEMASK (2), TVIS_STATEIMAGEMASK);
				pivotItems[p] = hItem;
			} else {
				// Pure bone — place under its parent item if parented to a sub-object/bone node,
				// else goes to the flat Bones folder.
				if (FindChildItem (hParentItem, pivotName) == nullptr) {
					int iconIdx = (hParentItem == hBonesFolder) ? m_iBoneIcon : m_iBoneIcon;
					HTREEITEM hItem = GetTreeCtrl ().InsertItem (pivotName, iconIdx, iconIdx, hParentItem, TVI_SORT);
					AssetInfoClass *asset_info = new AssetInfoClass (pivotName, TypeBone);
					GetTreeCtrl ().SetItemData (hItem, (ULONG)asset_info);
					pivotItems[p] = hItem;
				}
			}
		}

		// Sort the top-level Sub-Objects and Bones folders alphabetically.
		GetTreeCtrl ().SortChildren (hObjectsFolder);
		GetTreeCtrl ().SortChildren (hBonesFolder);

		// Release the refs held by the map (one Add_Ref per Get_Sub_Object call above)
		for (int i = 0; i < numSubs; ++i) {
			RenderObjClass *sub = robj->Get_Sub_Object (i);
			if (sub != nullptr) {
				sub->Release_Ref (); // release the map's ref
				sub->Release_Ref (); // release this loop's Get_Sub_Object ref
			}
		}

		robj->Release_Ref ();
	}

	SAFE_DELETE (pObjEnum);
}


////////////////////////////////////////////////////////////////////////////
//
//  LoadAssetsIntoTree
//
void
CDataTreeView::LoadAssetsIntoTree ()
{
	// TheSuperHackers @performance Tria 2026-05-19 Lock all window updates while we
	// bulk-mutate the tree. SetRedraw(FALSE) alone does not stop the control from
	// recomputing scrollbar ranges and item layout on every insert; LockWindowUpdate
	// prevents WM_PAINT / scrollbar / range traffic at the window-manager level. For
	// large folder loads (4000+ files) the visible "hang then recover" pause shrinks
	// dramatically.
	::LockWindowUpdate (GetTreeCtrl ().GetSafeHwnd ());

	// Turn off repainting
	GetTreeCtrl ().SetRedraw (FALSE);

	DynamicVectorClass<CString> dist_lod_list;

	// TheSuperHackers @performance Tria 18/04/2026 Build a hash set of existing tree items per parent
	// to replace O(n) FindChildItem scans with O(1) lookups. Also use TVI_LAST instead of TVI_SORT
	// to avoid O(n) sorted insertion per item, then sort each parent once at the end.
	HashTemplateClass<StringClass, int> existingMeshes;
	HashTemplateClass<StringClass, int> existingHierarchies;
	HashTemplateClass<StringClass, int> existingLODs;
	HashTemplateClass<StringClass, int> existingAggregates;
	HashTemplateClass<StringClass, int> existingEmitters;
	HashTemplateClass<StringClass, int> existingSounds;
	HashTemplateClass<StringClass, int> existingCollections;
	HashTemplateClass<StringClass, int> existingPrimitives;

	// Pre-populate hash sets with items already in the tree
	struct { HTREEITEM root; HashTemplateClass<StringClass, int> *set; } roots[] = {
		{ m_hMeshRoot, &existingMeshes },
		{ m_hHierarchyRoot, &existingHierarchies },
		{ m_hLODRoot, &existingLODs },
		{ m_hAggregateRoot, &existingAggregates },
		{ m_hEmitterRoot, &existingEmitters },
		{ m_hSoundRoot, &existingSounds },
		{ m_hMeshCollectionRoot, &existingCollections },
		{ m_hPrimitivesRoot, &existingPrimitives },
	};
	for (int r = 0; r < 8; ++r) {
		for (HTREEITEM h = GetTreeCtrl().GetChildItem(roots[r].root); h; h = GetTreeCtrl().GetNextSiblingItem(h)) {
			CString text = GetTreeCtrl().GetItemText(h);
			roots[r].set->Insert(StringClass((LPCTSTR)text), 1);
			// TheSuperHackers @feature Tria 19/04/2026 Scan grandchildren for grouped mesh items.
			if (roots[r].root == m_hMeshRoot) {
				for (HTREEITEM hChild = GetTreeCtrl().GetChildItem(h); hChild; hChild = GetTreeCtrl().GetNextSiblingItem(hChild)) {
					CString childText = GetTreeCtrl().GetItemText(hChild);
					roots[r].set->Insert(StringClass((LPCTSTR)childText), 1);
				}
			}
		}
	}

	// Track which parent nodes received new items and need re-sorting
	bool needsSort[8] = { false };

	// TheSuperHackers @feature Tria 19/04/2026 Group mesh items under their parent object name,
	// matching the hierarchy name style used by Bones and Materials.
	HashTemplateClass<StringClass, HTREEITEM> meshGroupNodes;
	for (HTREEITEM hGrp = GetTreeCtrl().GetChildItem(m_hMeshRoot); hGrp; hGrp = GetTreeCtrl().GetNextSiblingItem(hGrp)) {
		if (GetTreeCtrl().ItemHasChildren(hGrp)) {
			CString text = GetTreeCtrl().GetItemText(hGrp);
			meshGroupNodes.Insert(StringClass((LPCTSTR)text), hGrp);
		}
	}

	// Get an iterator from the asset manager that we can
	// use to enumerate the currently loaded assets
	RenderObjIterator *pObjEnum = WW3DAssetManager::Get_Instance()->Create_Render_Obj_Iterator ();
	ASSERT (pObjEnum);
	if (pObjEnum) {

		// Loop through all the assets in the manager
		for (pObjEnum->First ();
			  pObjEnum->Is_Done () == FALSE;
			  pObjEnum->Next ()) {

			// TheSuperHackers @performance Tria 2026-05-19 Removed redundant Render_Obj_Exists
			// hash lookup - iterator walks the same prototype list.
			LPCTSTR pszItemName = pObjEnum->Current_Item_Name ();
			{

				BOOL bInsert = FALSE;
				HTREEITEM hParentNode = nullptr;
				ASSET_TYPE assetType = TypeUnknown;
				int iIconIndex = -1;
				HashTemplateClass<StringClass, int> *pExistingSet = nullptr;
				int sortIndex = -1;

				// What type of asset is this?
				switch (pObjEnum->Current_Item_Class_ID ()) {

					case RenderObjClass::CLASSID_COLLECTION:
						bInsert = TRUE;
						hParentNode = m_hMeshCollectionRoot;
						assetType = TypeMesh;
						iIconIndex = m_iMeshIcon;
						pExistingSet = &existingCollections;
						sortIndex = 6;
						break;

					case RenderObjClass::CLASSID_MESH:
						bInsert			= TRUE;
						hParentNode		= m_hMeshRoot;
						assetType		= TypeMesh;
						iIconIndex		= m_iMeshIcon;
						pExistingSet = &existingMeshes;
						sortIndex = 0;
						break;

					case RenderObjClass::CLASSID_SOUND:
						bInsert			= TRUE;
						hParentNode		= m_hSoundRoot;
						assetType		= TypeSound;
						iIconIndex		= m_iSoundIcon;
						pExistingSet = &existingSounds;
						sortIndex = 5;
						break;

					case RenderObjClass::CLASSID_HMODEL:
						ASSERT (0);
						break;

					case RenderObjClass::CLASSID_PARTICLEEMITTER:
						bInsert = TRUE;
						hParentNode = m_hEmitterRoot;
						assetType = TypeEmitter;
						iIconIndex = m_iEmitterIcon;
						pExistingSet = &existingEmitters;
						sortIndex = 4;
						break;

					case RenderObjClass::CLASSID_SPHERE:
					case RenderObjClass::CLASSID_RING:
						bInsert		= TRUE;
						hParentNode = m_hPrimitivesRoot;
						assetType	= TypePrimitives;
						iIconIndex	= m_iPrimitivesIcon;
						pExistingSet = &existingPrimitives;
						sortIndex = 7;
						break;

					case RenderObjClass::CLASSID_DISTLOD:
						dist_lod_list.Add (pszItemName);
					case RenderObjClass::CLASSID_HLOD:
						bInsert = TRUE;
						hParentNode = m_hHierarchyRoot;
						assetType = TypeHierarchy;
						iIconIndex = m_iHierarchyIcon;
						pExistingSet = &existingHierarchies;
						sortIndex = 1;

						if (::Is_Real_LOD (pszItemName)) {
							hParentNode = m_hLODRoot;
							assetType = TypeLOD;
							iIconIndex = m_iLODIcon;
							pExistingSet = &existingLODs;
							sortIndex = 2;
						}
						break;
				}

				if (bInsert) {

					// Check to see if this object is an aggregate
					if (::Is_Aggregate (pszItemName)) {
						hParentNode = m_hAggregateRoot;
						assetType = TypeAggregate;
						iIconIndex = m_iAggregateIcon;
						pExistingSet = &existingAggregates;
						sortIndex = 3;
					}

					StringClass itemKey(pszItemName);

					// If this object isn't already in the tree then add it
					if (pExistingSet && !pExistingSet->Exists(itemKey)) {

						HTREEITEM hActualParent = hParentNode;

						// TheSuperHackers @feature Tria 19/04/2026 Group mesh items under their parent object name.
						if (hParentNode == m_hMeshRoot) {
							CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
							StringClass groupName ("Unknown");
							if (pdoc != nullptr) {
								const HashTemplateClass<StringClass, StringClass> &sourceMap = pdoc->GetAssetSourceFileMap ();
								if (sourceMap.Exists (itemKey)) {
									groupName = sourceMap.Get (itemKey);
									int len = groupName.Get_Length ();
									if (len > 4 && _stricmp ((LPCTSTR)groupName + len - 4, ".w3d") == 0)
										groupName.Erase (len - 4, 4);
								}
							}

							if (meshGroupNodes.Exists (groupName)) {
								hActualParent = meshGroupNodes.Get (groupName);
							} else {
								hActualParent = GetTreeCtrl ().InsertItem ((LPCTSTR)groupName, m_iHierarchyIcon, m_iHierarchyIcon, m_hMeshRoot, TVI_LAST);
								meshGroupNodes.Insert (groupName, hActualParent);
								needsSort[0] = true;
							}
						}

						// Add this entry to the tree (unsorted, will sort at end)
						HTREEITEM hItem = GetTreeCtrl ().InsertItem (pszItemName, iIconIndex, iIconIndex, hActualParent, TVI_LAST);
						ASSERT (hItem != nullptr);

						// Allocate a new asset information class to associate with this entry
						AssetInfoClass *asset_info = new AssetInfoClass (pszItemName, assetType);
						GetTreeCtrl ().SetItemData (hItem, (ULONG)asset_info);

						// TheSuperHackers @feature Tria 22/04/2026 For Hierarchy nodes, create the
						// Animations / Sub-Objects / Bones sub-folders immediately so LoadAnimationsIntoTree
						// and Load_Bones_Into_Tree can find them without a second pass.
						if (hParentNode == m_hHierarchyRoot || hParentNode == m_hLODRoot) {
							GetTreeCtrl ().InsertItem ("Animations",   m_iAnimationIcon, m_iAnimationIcon, hItem, TVI_LAST);
							HTREEITEM hSubObjFolder = GetTreeCtrl ().InsertItem ("Sub-Objects", m_iMeshIcon, m_iMeshIcon, hItem, TVI_LAST);
							GetTreeCtrl ().SetItemState (hSubObjFolder, INDEXTOSTATEIMAGEMASK (2), TVIS_STATEIMAGEMASK);
							GetTreeCtrl ().InsertItem ("Bones",        m_iBoneIcon,      m_iBoneIcon,      hItem, TVI_LAST);
						}

						// TheSuperHackers @feature Tria 18/04/2026 Show checkbox on mesh items for visibility toggling.
						if (hParentNode == m_hMeshRoot) {
							GetTreeCtrl ().SetItemState (hItem, INDEXTOSTATEIMAGEMASK (2), TVIS_STATEIMAGEMASK);
						}

						pExistingSet->Insert(itemKey, 1);
						if (sortIndex >= 0) needsSort[sortIndex] = true;
					}
				}
			}
		}

		// Free the enumerator object we created earlier
		SAFE_DELETE (pObjEnum);
	}

	// Loop through all the old-style dist lod's and convert their prototypes
	// to the new HLOD format
	for (int index = 0; index < dist_lod_list.Count (); index ++) {
		HLodClass *plod = (HLodClass *)WW3DAssetManager::Get_Instance ()->Create_Render_Obj (dist_lod_list[index]);
		if (plod != nullptr) {
			HLodDefClass *definition = new HLodDefClass (*plod);
			HLodPrototypeClass *prototype = new HLodPrototypeClass (definition);
			WW3DAssetManager::Get_Instance ()->Remove_Prototype (dist_lod_list[index]);
			WW3DAssetManager::Get_Instance ()->Add_Prototype (prototype);
		}
	}

	// Now that we've added all the hierarchies to the tree, add their animations
	// as well.
	LoadAnimationsIntoTree ();
	Load_Materials_Into_Tree ();
	Load_Bones_Into_Tree ();

	// TheSuperHackers @performance Tria 18/04/2026 Sort each parent node once instead of per-insert.
	for (int r = 0; r < 8; ++r) {
		if (needsSort[r]) {
			GetTreeCtrl().SortChildren(roots[r].root);
		}
	}

	// Turn repainting back on
	GetTreeCtrl ().SetRedraw (TRUE);

	// TheSuperHackers @performance Tria 2026-05-19 Release the window-update lock
	// before invalidating so the final repaint actually happens.
	::LockWindowUpdate (nullptr);

	// Force the window to be repainted
	Invalidate (FALSE);
	UpdateWindow ();
}

////////////////////////////////////////////////////////////////////////////
//
//  LoadAnimationsIntoTree
//
void
CDataTreeView::LoadAnimationsIntoTree ()
{
    // TheSuperHackers @performance Tria 18/04/2026 Build maps of hierarchy name to tree nodes once
    // instead of doing O(n) linear scans per animation. Also use hash sets for child dedup and
    // TVI_LAST instead of TVI_SORT, with a single sort pass at the end.

    // Helper: collect child nodes grouped by hierarchy name for a given root
    struct NodeList {
        DynamicVectorClass<HTREEITEM> items;
    };

    // Build hierarchy name -> node list maps for the three root categories
    HTREEITEM rootNodes[3] = { m_hHierarchyRoot, m_hAggregateRoot, m_hLODRoot };
    HashTemplateClass<StringClass, NodeList*> rootMaps[3];

    for (int r = 0; r < 3; ++r) {
        for (HTREEITEM h = GetTreeCtrl().GetChildItem(rootNodes[r]); h; h = GetTreeCtrl().GetNextSiblingItem(h)) {
            AssetInfoClass *info = (AssetInfoClass *)GetTreeCtrl().GetItemData(h);
            StringClass key;
            if (m_RestrictAnims && info) {
                key = (LPCTSTR)info->Get_Hierarchy_Name();
            } else {
                key = "*";  // match-all sentinel when not restricting
            }
            NodeList *list = nullptr;
            if (rootMaps[r].Exists(key)) {
                list = rootMaps[r].Get(key);
            } else {
                list = new NodeList();
                rootMaps[r].Insert(key, list);
            }
            list->items.Add(h);
        }
    }

    // Track which hierarchy nodes got new animation children and need sorting
    HashTemplateClass<HTREEITEM, int> nodesToSort;

    // Get an iterator from the asset manager that we can
    // use to enumerate the currently loaded assets
    AssetIterator *pAnimEnum = WW3DAssetManager::Get_Instance()->Create_HAnim_Iterator ();
    ASSERT (pAnimEnum);
    if (pAnimEnum)
    {
        // Loop through all the animations in the manager
        for (pAnimEnum->First ();
             (pAnimEnum->Is_Done () == FALSE);
             pAnimEnum->Next ())
        {
            LPCTSTR pszAnimName = pAnimEnum->Current_Item_Name ();

            // Get an instance of the animation object
            HAnimClass *pHierarchyAnim = WW3DAssetManager::Get_Instance()->Get_HAnim (pszAnimName);

            ASSERT (pHierarchyAnim);
            if (pHierarchyAnim)
            {
                // Get the name of the hierarchy that this animation belongs to
                LPCTSTR pszHierarchyName = pHierarchyAnim->Get_HName ();
                StringClass lookupKey;
                if (m_RestrictAnims) {
                    lookupKey = pszHierarchyName;
                } else {
                    lookupKey = "*";
                }

                // Insert animation into matching nodes across all three root categories
                for (int r = 0; r < 3; ++r) {
                    if (!rootMaps[r].Exists(lookupKey)) continue;
                    NodeList *list = rootMaps[r].Get(lookupKey);
                    for (int i = 0; i < list->items.Count(); ++i) {
                        HTREEITEM hNode = list->items[i];

                        // TheSuperHackers @feature Tria 22/04/2026 For Hierarchy/LOD nodes, put
                        // animations under their Animations sub-folder. Aggregates get them directly.
                        HTREEITEM hTarget = hNode;
                        if (r != 1) { // r==1 is Aggregate — no sub-folders
                            HTREEITEM hAnimFolder = FindChildItem (hNode, "Animations");
                            if (hAnimFolder != nullptr)
                                hTarget = hAnimFolder;
                        }

                        // Check if animation already exists under the target node
                        HTREEITEM hAnimationNode = FindChildItem (hTarget, pszAnimName);
                        if (hAnimationNode == nullptr)
                        {
                            hAnimationNode = GetTreeCtrl ().InsertItem (pszAnimName, m_iAnimationIcon, m_iAnimationIcon, hTarget, TVI_LAST);
                            ASSERT (hAnimationNode != nullptr);
                            GetTreeCtrl ().SetItemData (hAnimationNode, (ULONG)new AssetInfoClass (pszAnimName, TypeAnimation));
                            if (!nodesToSort.Exists(hTarget)) {
                                nodesToSort.Insert(hTarget, 1);
                            }
                        }
                    }
                }

                // Release our hold on this animation...
					 REF_PTR_RELEASE (pHierarchyAnim);
            }
        }

        // Free the object
        delete pAnimEnum;
        pAnimEnum = nullptr;
    }

    // Sort nodes that received new animation children
    HashTemplateIterator<HTREEITEM, int> sortIte(nodesToSort);
    for (sortIte.First(); !sortIte.Is_Done(); sortIte.Next()) {
        GetTreeCtrl().SortChildren(sortIte.Peek_Key());
    }

    // Clean up node lists
    for (int r = 0; r < 3; ++r) {
        HashTemplateIterator<StringClass, NodeList*> ite(rootMaps[r]);
        for (ite.First(); !ite.Is_Done(); ite.Next()) {
            delete ite.Peek_Value();
        }
    }

}

////////////////////////////////////////////////////////////////////////////
//
//  LoadAnimationsIntoTree
//
void
CDataTreeView::LoadAnimationsIntoTree (HTREEITEM hItem)
{
    // Get the data associated with this item
    AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hItem);
    ASSERT (asset_info != nullptr);

    // Get an iterator from the asset manager that we can
    // use to enumerate the currently loaded assets
    AssetIterator *pAnimEnum = WW3DAssetManager::Get_Instance()->Create_HAnim_Iterator ();
    ASSERT (pAnimEnum);
    if (pAnimEnum)
    {
        // Loop through all the animations in the manager
        for (pAnimEnum->First ();
             (pAnimEnum->Is_Done () == FALSE);
             pAnimEnum->Next ())
        {
            LPCTSTR pszAnimName = pAnimEnum->Current_Item_Name ();

            // Get an instance of the animation object
            HAnimClass *pHierarchyAnim = WW3DAssetManager::Get_Instance()->Get_HAnim (pszAnimName);

            ASSERT (pHierarchyAnim);
            if (pHierarchyAnim)
            {
                // Get the name of the hierarchy that this animation belongs to
                LPCTSTR pszHierarchyName = pHierarchyAnim->Get_HName ();

                // Does the item match the hierarchy name?
                if (::lstrcmp (asset_info->Get_Hierarchy_Name (), pszHierarchyName) == 0)
                {
                    // Is this animation already loaded into the tree?
                    HTREEITEM hAnimationNode = FindChildItem (hItem, pszAnimName);
                    if (hAnimationNode == nullptr)
                    {
                        // Add this animation as a child of the hierarchy
                        hAnimationNode = GetTreeCtrl ().InsertItem (pszAnimName, m_iAnimationIcon, m_iAnimationIcon, hItem, TVI_SORT);
                        ASSERT (hAnimationNode != nullptr);

                        // Associate the items name with its entry
                        GetTreeCtrl ().SetItemData (hAnimationNode, (ULONG)new AssetInfoClass (pszAnimName, TypeAnimation));
                    }
                }

                // Release our hold on the animation object
                REF_PTR_RELEASE (pHierarchyAnim);
            }
        }

        // Free the object
        delete pAnimEnum;
        pAnimEnum = nullptr;
    }
}


////////////////////////////////////////////////////////////////////////////
//
//  Determine_Tree_Location
//
ASSET_TYPE
CDataTreeView::Determine_Tree_Location
(
	RenderObjClass &render_obj,
	HTREEITEM &hroot,
	int &icon_index
)
{
	ASSET_TYPE type = TypeUnknown;

	// What class does this render object belong to?
	switch (render_obj.Class_ID ()) {

		case RenderObjClass::CLASSID_COLLECTION:
			hroot				= m_hMeshCollectionRoot;
			type				= TypeMesh;
			icon_index		= m_iMeshIcon;
			break;

		case RenderObjClass::CLASSID_MESH:
			hroot = m_hMeshRoot;
			type = TypeMesh;
			icon_index = m_iMeshIcon;
			break;

		case RenderObjClass::CLASSID_SOUND:
			hroot				= m_hSoundRoot;
			type				= TypeSound;
			icon_index		= m_iSoundIcon;
			break;

		case RenderObjClass::CLASSID_HMODEL:
			// Shouldn't happen
			ASSERT (0);
			break;

		case RenderObjClass::CLASSID_PARTICLEEMITTER:
			hroot				= m_hEmitterRoot;
			type				= TypeEmitter;
			icon_index		= m_iEmitterIcon;
			break;

		case RenderObjClass::CLASSID_SPHERE:
		case RenderObjClass::CLASSID_RING:
			hroot				= m_hPrimitivesRoot;
			type				= TypePrimitives;
			icon_index		= m_iPrimitivesIcon;
			break;

		case RenderObjClass::CLASSID_DISTLOD:
		case RenderObjClass::CLASSID_HLOD:
         hroot				= m_hHierarchyRoot;
         type				= TypeHierarchy;
         icon_index		= m_iHierarchyIcon;

			//
			// Determine if this is a true LOD or a simple hierarchy
			//
			if (((HLodClass &)render_obj).Get_LOD_Count () > 1) {
				hroot			= m_hLODRoot;
				type			= TypeLOD;
				icon_index	= m_iLODIcon;
			}
			break;
	}

	//
	// Is this an aggregate?
	//
	if (render_obj.Get_Base_Model_Name () != nullptr) {
		hroot			= m_hAggregateRoot;
		type			= TypeAggregate;
		icon_index	= m_iAggregateIcon;
	}

	// Return the type of node
	return type;
}



////////////////////////////////////////////////////////////////////////////
//
//  Determine_Tree_Location
//
void
CDataTreeView::Determine_Tree_Location
(
	ASSET_TYPE type,
	HTREEITEM &hroot,
	int &icon_index
)
{
	// What type of asset is this?
	switch (type) {

		case TypeMesh:
			hroot			= m_hMeshRoot;
			icon_index	= m_iMeshIcon;
			break;

		case TypeSound:
			hroot			= m_hSoundRoot;
			icon_index	= m_iSoundIcon;
			break;

		case TypeAggregate:
			hroot			= m_hAggregateRoot;
			icon_index	= m_iAggregateIcon;
			break;

		case TypeHierarchy:
			// Shouldn't happen
			ASSERT (0);
			break;

		case TypeEmitter:
			hroot			= m_hEmitterRoot;
			icon_index	= m_iEmitterIcon;
			break;

		case TypePrimitives:
			hroot			= m_hPrimitivesRoot;
			icon_index	= m_iPrimitivesIcon;
			break;

		case TypeLOD:
			hroot			= m_hLODRoot;
			icon_index	= m_iLODIcon;
			break;
	}
}



////////////////////////////////////////////////////////////////////////////
//
//  Add_Asset_To_Tree
//
bool
CDataTreeView::Add_Asset_To_Tree
(
	LPCTSTR name,
	ASSET_TYPE type,
	bool bselect
)
{
	// Assume failure
	bool retval = false;

	// Param OK?
	ASSERT (name != nullptr);
	if (name != nullptr) {

		// Turn off repainting
		GetTreeCtrl ().SetRedraw (FALSE);

		// Determime where this asset should go
		HTREEITEM hparent = nullptr;
		int icon_index = 0;
		Determine_Tree_Location (type, hparent, icon_index);

		// TheSuperHackers @feature Tria 19/04/2026 For mesh items, resolve to the proper group node.
		if (type == TypeMesh && hparent == m_hMeshRoot) {
			CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
			StringClass groupName ("Unknown");
			if (pdoc != nullptr) {
				StringClass nameKey (name);
				const HashTemplateClass<StringClass, StringClass> &sourceMap = pdoc->GetAssetSourceFileMap ();
				if (sourceMap.Exists (nameKey)) {
					groupName = sourceMap.Get (nameKey);
					int len = groupName.Get_Length ();
					if (len > 4 && _stricmp ((LPCTSTR)groupName + len - 4, ".w3d") == 0)
						groupName.Erase (len - 4, 4);
				}
			}

			// Find existing group node or create one
			HTREEITEM hGroupNode = nullptr;
			for (HTREEITEM h = GetTreeCtrl().GetChildItem(m_hMeshRoot); h; h = GetTreeCtrl().GetNextSiblingItem(h)) {
				CString text = GetTreeCtrl().GetItemText(h);
				if (StringClass((LPCTSTR)text) == groupName) {
					hGroupNode = h;
					break;
				}
			}
			if (hGroupNode == nullptr) {
				hGroupNode = GetTreeCtrl ().InsertItem ((LPCTSTR)groupName, m_iHierarchyIcon, m_iHierarchyIcon, m_hMeshRoot, TVI_SORT);
			}
			hparent = hGroupNode;
		}

		// Is this asset already in the tree?
		HTREEITEM htree_item = FindChildItem (hparent, name);
		if (htree_item == nullptr) {

			// Add this object to the tree
			htree_item = GetTreeCtrl ().InsertItem (name,
																 icon_index,
																 icon_index,
																 hparent,
																 TVI_SORT);

			// Associate the render object with its entry in the tree
			AssetInfoClass *asset_info = new AssetInfoClass (name, type);
			GetTreeCtrl ().SetItemData (htree_item, (ULONG)asset_info);

			// TheSuperHackers @feature Tria 18/04/2026 Show checkbox on mesh items for visibility toggling.
			// TheSuperHackers @bugfix Tria 22/04/2026 Also set checked for meshes nested under a group node (child of MeshRoot).
			if (type == TypeMesh && (hparent == m_hMeshRoot || GetTreeCtrl ().GetParentItem (hparent) == m_hMeshRoot)) {
				GetTreeCtrl ().SetItemState (htree_item, INDEXTOSTATEIMAGEMASK (2), TVIS_STATEIMAGEMASK);
			}

			// Load the object's animations into the tree (if necessary)
			if (asset_info->Can_Asset_Have_Animations ()) {
				LoadAnimationsIntoTree (htree_item);
			}

			// Success!
			retval = (htree_item != nullptr);
		}

		// Select the instance (if requested)
		if (bselect) {
			GetTreeCtrl ().SelectItem (htree_item);
			GetTreeCtrl ().EnsureVisible (htree_item);
		}

		// Turn painting back on
		GetTreeCtrl ().SetRedraw (TRUE);

		// Force the window to be repainted
		Invalidate (FALSE);
		UpdateWindow ();
	}

	// Return the true/false result code
	return retval;
}


////////////////////////////////////////////////////////////////////////////
//
//  FindChildItem
//
HTREEITEM
CDataTreeView::FindChildItem
(
	HTREEITEM hParentItem,
	RenderObjClass *prender_obj
)
{
	// Assume we won't find the item
	HTREEITEM hchild_item = nullptr;

	// Loop through all the children of this node
	for (HTREEITEM htree_item = GetTreeCtrl ().GetChildItem (hParentItem);
		  (htree_item != nullptr) && (hchild_item == nullptr);
		  htree_item = GetTreeCtrl ().GetNextSiblingItem (htree_item)) {

		// Get the data associated with this item
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);

		// Is this the item we were looking for?
		if (asset_info &&
			 asset_info->Peek_Render_Obj () == prender_obj) {

			// This was the item we were looking for, return
			// its handle to the caller
			hchild_item = htree_item;
		}
	}

	// Return the child item handle
	return hchild_item;
}


////////////////////////////////////////////////////////////////////////////
//
//  FindChildItem
//
HTREEITEM
CDataTreeView::FindChildItem
(
    HTREEITEM hParentItem,
    LPCTSTR pszChildItemName
)
{
    // Assume we won't find the item
    HTREEITEM hChildItem = nullptr;

    // Loop through all the children of this node
    for (HTREEITEM hTreeItem = GetTreeCtrl ().GetChildItem (hParentItem);
         (hTreeItem != nullptr) && (hChildItem == nullptr);
         hTreeItem = GetTreeCtrl ().GetNextSiblingItem (hTreeItem))
    {
        // Is this the child item we were looking for?
        if (::lstrcmp (GetTreeCtrl ().GetItemText (hTreeItem), pszChildItemName) == 0)
        {
            // This was the child item we were looking for, return
            // its handle to the caller
            hChildItem = hTreeItem;
        }
    }

    // Return the child item handle
    return hChildItem;
}


////////////////////////////////////////////////////////////////////////////
//
//  Find_Asset_Item_By_Name
//
//  Walks the whole tree (recursively, all roots) and returns the first
//  HTREEITEM whose AssetInfoClass::Get_Name() matches pszName. Used by
//  Reload-Assets restore (MainFrm.cpp) to re-select the previously
//  displayed asset after the tree has been rebuilt.
//
////////////////////////////////////////////////////////////////////////////
static HTREEITEM
Find_Asset_Item_By_Name_Recursive (CTreeCtrl &tree, HTREEITEM hparent, LPCTSTR pszName)
{
	for (HTREEITEM htree_item = tree.GetChildItem (hparent);
		  htree_item != nullptr;
		  htree_item = tree.GetNextSiblingItem (htree_item)) {

		AssetInfoClass *asset_info = (AssetInfoClass *)tree.GetItemData (htree_item);
		if (asset_info != nullptr && ::lstrcmpi (asset_info->Get_Name (), pszName) == 0) {
			return htree_item;
		}

		if (tree.ItemHasChildren (htree_item)) {
			HTREEITEM hfound = Find_Asset_Item_By_Name_Recursive (tree, htree_item, pszName);
			if (hfound != nullptr) {
				return hfound;
			}
		}
	}

	return nullptr;
}

HTREEITEM
CDataTreeView::Find_Asset_Item_By_Name (LPCTSTR pszName)
{
	if (pszName == nullptr || pszName[0] == '\0') {
		return nullptr;
	}
	return Find_Asset_Item_By_Name_Recursive (GetTreeCtrl (), TVI_ROOT, pszName);
}


////////////////////////////////////////////////////////////////////////////
//
//  FindSiblingItemBasedOnHierarchyName
//
HTREEITEM
CDataTreeView::FindSiblingItemBasedOnHierarchyName
(
    HTREEITEM hCurrentItem,
    LPCTSTR pszHierarchyName
)
{
    // Assume we won't find the item
    HTREEITEM hSiblingItem = nullptr;

    // Loop through all the siblings of this node
    HTREEITEM hTreeItem = hCurrentItem;
    while (((hTreeItem = GetTreeCtrl ().GetNextSiblingItem (hTreeItem)) != nullptr) &&
           (hSiblingItem == nullptr))
    {
        // Get the data associated with this item
        AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hTreeItem);

        // Is this the item we were looking for?
        if (m_RestrictAnims == false ||
				(asset_info && ::lstrcmp (asset_info->Get_Hierarchy_Name (), pszHierarchyName) == 0))
        {
            // This was the item we were looking for, return
            // its handle to the caller
            hSiblingItem = hTreeItem;
        }
    }

    // Return the sibling item handle
    return hSiblingItem;
}

////////////////////////////////////////////////////////////////////////////
//
//  FindFirstChildItemBasedOnHierarchyName
//
HTREEITEM
CDataTreeView::FindFirstChildItemBasedOnHierarchyName
(
    HTREEITEM hParentItem,
    LPCTSTR pszHierarchyName
)
{
    // Assume we won't find the item
    HTREEITEM hChildItem = nullptr;

    // Loop through all the children of this node
    for (HTREEITEM hTreeItem = GetTreeCtrl ().GetChildItem (hParentItem);
         (hTreeItem != nullptr) && (hChildItem == nullptr);
         hTreeItem = GetTreeCtrl ().GetNextSiblingItem (hTreeItem))
    {
        // Get the data associated with this item
        AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hTreeItem);

        //
		  // Is this the item we were looking for?
		  //
        if (m_RestrictAnims == false ||
				(asset_info && ::lstrcmp (asset_info->Get_Hierarchy_Name (), pszHierarchyName) == 0))
        {
            // This was the child item we were looking for, return
            // its handle to the caller
            hChildItem = hTreeItem;
        }
    }

    // Return the child item handle
    return hChildItem;
}


////////////////////////////////////////////////////////////////////////////
//
//  OnSelChanged
//
void
CDataTreeView::OnSelChanged
(
    NMHDR* pNMHDR,
    LRESULT* pResult
)
{
	// Display the new selection
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	Display_Asset (pNMTreeView->itemNew.hItem);

	// TheSuperHackers @feature Tria 18/04/2026 Update the title bar with the selected asset name.
	HTREEITEM hItem = pNMTreeView->itemNew.hItem;
	if (hItem != nullptr) {
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hItem);
		if (asset_info != nullptr) {
			CDocument *pDoc = GetDocument ();
			if (pDoc != nullptr) {
				pDoc->SetTitle (asset_info->Get_Name ());
			}
		}
	}

	(*pResult) = 0;
}


////////////////////////////////////////////////////////////////////////////
//
//  Display_Asset
//
void
CDataTreeView::Display_Asset (HTREEITEM htree_item)
{
	// Clear multi-select set whenever the user navigates to a new asset.
	m_selectedMeshItems.Delete_All ();
	m_hLastMultiSelectAnchor = nullptr;

	if (htree_item == nullptr) {
		htree_item = GetTreeCtrl ().GetSelectedItem ();
	}

	//
	// Get the object associated with this entry
	//
	AssetInfoClass *asset_info = nullptr;
	if (htree_item != nullptr) {
		asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
	}

	if (asset_info != nullptr) {

		// Get the current document, so we can get a pointer to the scene
		CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
		ASSERT (pdoc != nullptr);
		if (pdoc != nullptr) {

			// What type of asset is it?
			switch (asset_info->Get_Type ())
			{
				case TypeCompressedAnimation:
				case TypeAnimation:
				{
					// TheSuperHackers @feature Tria 22/04/2026 Animations are now under an
					// "Animations" sub-folder; walk up past it to reach the hierarchy node.
					HTREEITEM hParentItem = GetTreeCtrl ().GetParentItem (htree_item);
					if (hParentItem != nullptr) {
						CString folderText = GetTreeCtrl ().GetItemText (hParentItem);
						if (folderText == "Animations") {
							hParentItem = GetTreeCtrl ().GetParentItem (hParentItem);
						}
					}
					if (hParentItem != nullptr) {

						// Ask the document to start playing the animation for this object
						RenderObjClass *prender_obj = Create_Render_Obj_To_Display (hParentItem);
						pdoc->PlayAnimation (prender_obj,
						asset_info->Get_Name ());
						REF_PTR_RELEASE (prender_obj);
					}
				}
				break;

				case TypeEmitter:
				{
					// Ask the document to display this object
					ParticleEmitterClass *emitter = (ParticleEmitterClass *)Create_Render_Obj_To_Display (htree_item);
					pdoc->Display_Emitter (emitter);
					REF_PTR_RELEASE (emitter);
				}
				break;

				case TypeHierarchy:
				{
					// Display the hierarchy object and clear any sub-item selection.
					RenderObjClass *prender_obj = Create_Render_Obj_To_Display (htree_item);
					pdoc->DisplayObject (prender_obj);
					REF_PTR_RELEASE (prender_obj);
				}
				break;

				case TypeBone:
				{
					// TheSuperHackers @feature Tria 23/04/2026 Selecting a bone in the tree highlights
					// it in the viewport — no object switch, just update the selection tracker.
					pdoc->SetSelectedItem (asset_info->Get_Name (), TypeBone);
					return;
				}

				case TypeMesh:
				{
					// TheSuperHackers @feature Tria 23/04/2026 Selecting a sub-object under a hierarchy
					// Sub-Objects folder highlights its pivot in the viewport. Items under the flat Mesh
					// root still call DisplayObject as before.
					// Walk up to check if this item is anywhere inside a Sub-Objects folder
					// (handles nested sub-objects at any depth).
					bool isHierarchySubObj = false;
					for (HTREEITEM hAnc = GetTreeCtrl ().GetParentItem (htree_item); hAnc != nullptr; hAnc = GetTreeCtrl ().GetParentItem (hAnc)) {
						CString text = GetTreeCtrl ().GetItemText (hAnc);
						if (text == "Sub-Objects") { isHierarchySubObj = true; break; }
					}
					if (isHierarchySubObj) {
						pdoc->SetSelectedItem (asset_info->Get_Name (), TypeMesh);
						return;
					}
					// Standalone mesh item — display it directly
					RenderObjClass *prender_obj = Create_Render_Obj_To_Display (htree_item);
					pdoc->DisplayObject (prender_obj);
					REF_PTR_RELEASE (prender_obj);
				}
				break;

				default:
				{
					// Ask the document to display this object
					RenderObjClass *prender_obj = Create_Render_Obj_To_Display (htree_item);
					pdoc->DisplayObject (prender_obj);
					REF_PTR_RELEASE (prender_obj);
				}
				break;
			}

			// Get the main window of our app
			CMainFrame *pCMainWnd = (CMainFrame *)::AfxGetMainWnd ();
			if (pCMainWnd != nullptr) {

				// Let the main window know our selection type has changed
				pCMainWnd->OnSelectionChanged (asset_info->Get_Type ());

				if (asset_info->Get_Type () == TypeAggregate) {
					pCMainWnd->Update_Emitters_List ();
				} else {

					::EnableMenuItem (::GetSubMenu (::GetMenu (*pCMainWnd), 3), 3, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					HMENU hsub_menu = pCMainWnd->Get_Emitters_List_Menu ();
					int index = 0;
					while (::RemoveMenu (hsub_menu, index, MF_BYPOSITION)) {
						//index ++;
					}
				}
			}
		}
	} else {

		CW3DViewDoc* pdoc = (CW3DViewDoc *)GetDocument ();
		ASSERT (pdoc != nullptr);
		if (pdoc != nullptr) {

			// TheSuperHackers @feature Tria 23/04/2026 Clicking a Sub-Objects or Bones folder header
			// just clears the per-item selection without switching the displayed object.
			if (htree_item != nullptr) {
				CString itemText = GetTreeCtrl ().GetItemText (htree_item);
				if (itemText == "Sub-Objects" || itemText == "Bones") {
					pdoc->ClearSelectedItem ();
					return;
				}
			}

			// TheSuperHackers @feature Tria 22/04/2026 When the user clicks a Hierarchy child node
			// (direct child of Hierarchy root, no asset_info yet assigned), display its render object.
			if (htree_item != nullptr && GetTreeCtrl ().GetParentItem (htree_item) == m_hHierarchyRoot) {
				CString groupLabel = GetTreeCtrl ().GetItemText (htree_item);
				RenderObjClass *prender_obj = WW3DAssetManager::Get_Instance ()->Create_Render_Obj ((LPCTSTR)groupLabel);
				if (prender_obj != nullptr) {
					if (::GetCurrentDocument ()->GetScene ()->Are_LODs_Switching () == false)
						Set_Highest_LOD (prender_obj);
					pdoc->DisplayObject (prender_obj);
					REF_PTR_RELEASE (prender_obj);

					// Reset checkboxes on all Sub-Objects sub-folder children to checked (visible).
					HTREEITEM hObjectsFolder = FindChildItem (htree_item, "Sub-Objects");
					if (hObjectsFolder != nullptr) {
						for (HTREEITEM hChild = GetTreeCtrl ().GetChildItem (hObjectsFolder); hChild; hChild = GetTreeCtrl ().GetNextSiblingItem (hChild))
							GetTreeCtrl ().SetItemState (hChild, INDEXTOSTATEIMAGEMASK (2), TVIS_STATEIMAGEMASK);
					}

					CMainFrame *main_wnd = (CMainFrame *)::AfxGetMainWnd ();
					if (main_wnd != nullptr)
						main_wnd->OnSelectionChanged (TypeHierarchy);
					return;
				}
			}

			// Reset the display
			pdoc->DisplayObject ((RenderObjClass *)nullptr);

			CMainFrame *main_wnd = (CMainFrame *)::AfxGetMainWnd ();
			if (main_wnd != nullptr) {
				main_wnd->OnSelectionChanged (TypeUnknown);
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  OnRClick
//
// TheSuperHackers @feature Tria 18/04/2026 Right-click context menu to toggle mesh visibility
// on the currently displayed model. Allows hiding/showing individual sub-meshes.
//
void
CDataTreeView::OnRClick (NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 1; // Prevent default processing

	// Hit-test to find which item was right-clicked
	CPoint pt;
	::GetCursorPos (&pt);
	CPoint clientPt = pt;
	GetTreeCtrl ().ScreenToClient (&clientPt);

	UINT uFlags = 0;
	HTREEITEM hItem = GetTreeCtrl ().HitTest (clientPt, &uFlags);
	if (hItem == nullptr)
		return;

	CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
	if (pdoc == nullptr)
		return;

	// TheSuperHackers @feature Tria 19/04/2026 Right-click on Hierarchy root or any hierarchy item
	// to export texture and mesh info for the selected object.
	if (hItem == m_hHierarchyRoot || GetTreeCtrl ().GetParentItem (hItem) == m_hHierarchyRoot) {
		// Only offer menu on actual hierarchy items, not the root label
		if (hItem != m_hHierarchyRoot) {
			m_hRightClickItem = hItem;
			CMenu menu;
			menu.CreatePopupMenu ();
			menu.AppendMenu (MF_STRING, IDM_DUMP_W3D_INFO, "Dump W3D Info");
			menu.AppendMenu (MF_STRING, IDM_EXPORT_INFO,   "Export Info");
			menu.AppendMenu (MF_SEPARATOR);
			menu.AppendMenu (MF_STRING, IDM_MATVIEWER_FROM_TREE, "Open in Material Viewer");
			menu.TrackPopupMenu (TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
		}
		return;
	}

	// Only act on mesh sub-object items (under Objects sub-folder or direct Mesh root children)
	if (!Is_Mesh_Sub_Object (hItem)) {

		// TheSuperHackers @feature Standalone mesh assets (e.g. under the Mesh
		// category root) still get the Material Viewer entry.
		AssetInfoClass *standalone_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hItem);
		if (standalone_info != nullptr && standalone_info->Get_Type () == TypeMesh) {
			m_hRightClickItem = hItem;
			CMenu menu;
			menu.CreatePopupMenu ();
			menu.AppendMenu (MF_STRING, IDM_MATVIEWER_FROM_TREE, "Open in Material Viewer");
			menu.TrackPopupMenu (TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
		}
		return;
	}

	AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hItem);
	if (asset_info == nullptr)
		return;

	// Store the right-clicked item for OnToggleMeshVis / OnOpenInMaterialViewer
	m_hRightClickItem = hItem;

	// Build context menu: the visibility toggle only applies when the mesh is a
	// sub-object of the currently displayed model; the Material Viewer entry is
	// always available.
	CMenu menu;
	menu.CreatePopupMenu ();

	RenderObjClass *pDisplayed = pdoc->GetDisplayedObject ();
	if (pDisplayed != nullptr) {
		const char *meshName = asset_info->Get_Name ();
		int subIndex = -1;
		RenderObjClass *pSubObj = pDisplayed->Get_Sub_Object_By_Name (meshName, &subIndex);
		if (pSubObj != nullptr) {
			if (pSubObj->Is_Hidden ()) {
				menu.AppendMenu (MF_STRING, IDM_TOGGLE_MESH_VIS, "Show Mesh");
			} else {
				menu.AppendMenu (MF_STRING, IDM_TOGGLE_MESH_VIS, "Hide Mesh");
			}
			menu.AppendMenu (MF_SEPARATOR);
			pSubObj->Release_Ref ();
		}
	}

	menu.AppendMenu (MF_STRING, IDM_MATVIEWER_FROM_TREE, "Open in Material Viewer");

	menu.TrackPopupMenu (TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
}


////////////////////////////////////////////////////////////////////////////
//
//  Is_Mesh_Sub_Object
//
// TheSuperHackers @feature Tria 22/04/2026 Returns true if the given tree item is a mesh
// sub-object that supports visibility toggling. This covers items under the standalone Mesh root
// group nodes and items under the Objects sub-folder of Hierarchy nodes.
//
bool
CDataTreeView::Is_Mesh_Sub_Object (HTREEITEM hItem) const
{
	if (hItem == nullptr)
		return false;
	HTREEITEM hParent = GetTreeCtrl ().GetParentItem (hItem);
	if (hParent == nullptr)
		return false;

	// Direct child of m_hMeshRoot or grandchild (under a group node)
	if (hParent == m_hMeshRoot)
		return true;
	if (GetTreeCtrl ().GetParentItem (hParent) == m_hMeshRoot)
		return true;

	// Walk up the tree looking for a "Sub-Objects" folder whose parent is a hierarchy node.
	// This handles sub-objects nested at any depth under the Sub-Objects folder.
	HTREEITEM hAncestor = hParent;
	while (hAncestor != nullptr) {
		CString text = GetTreeCtrl ().GetItemText (hAncestor);
		if (text == "Sub-Objects") {
			HTREEITEM hHierNode = GetTreeCtrl ().GetParentItem (hAncestor);
			if (hHierNode != nullptr && GetTreeCtrl ().GetParentItem (hHierNode) == m_hHierarchyRoot)
				return true;
			break;
		}
		hAncestor = GetTreeCtrl ().GetParentItem (hAncestor);
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
// TheSuperHackers @feature Tria 18/04/2026 Intercept left-click on eye state icon to toggle
// mesh visibility before the tree control changes the selection.
// TheSuperHackers @feature Tria 23/04/2026 Ctrl+click on item label adds/removes it from the
// multi-select set. Clicking the state icon toggles all selected items at once.
// TheSuperHackers @feature Tria 23/04/2026 Shift+click range-selects siblings between the last
// anchor and the clicked item and toggles them with their sub-objects.
//
void
CDataTreeView::OnLButtonDown (UINT nFlags, CPoint point)
{
	UINT uFlags = 0;
	HTREEITEM hItem = GetTreeCtrl ().HitTest (point, &uFlags);

	// Allow clicking the state icon on the Sub-Objects folder itself to cascade to all children.
	if (hItem != nullptr && (uFlags & TVHT_ONITEMSTATEICON)) {
		CString itemText = GetTreeCtrl ().GetItemText (hItem);
		if (itemText == "Sub-Objects") {
			// Determine target visibility from first child's state.
			HTREEITEM hFirst = GetTreeCtrl ().GetChildItem (hItem);
			bool newVisible = true;
			if (hFirst != nullptr) {
				UINT st = GetTreeCtrl ().GetItemState (hFirst, TVIS_STATEIMAGEMASK);
				newVisible = ((st >> 12) & 0xF) != 2; // invert current first child state
			}
			for (HTREEITEM hChild = GetTreeCtrl ().GetChildItem (hItem); hChild; hChild = GetTreeCtrl ().GetNextSiblingItem (hChild))
				Set_Mesh_Visibility_Recursive (hChild, newVisible);
			return;
		}
	}

	if (hItem != nullptr && Is_Mesh_Sub_Object (hItem)) {
		if (uFlags & TVHT_ONITEMSTATEICON) {
			// Toggling the checkbox — apply to multi-select set if populated, else single/group toggle.
			Toggle_Mesh_Visibility (hItem);
			return; // eat the click
		}

		if ((uFlags & TVHT_ONITEM) && (nFlags & MK_SHIFT) && m_hLastMultiSelectAnchor != nullptr) {
			// Shift+click: replace the multi-select set with the range [anchor, hItem] among siblings.
			// The anchor does NOT move (matches Windows Explorer behavior).
			HTREEITEM hParent = GetTreeCtrl ().GetParentItem (hItem);
			HTREEITEM hAnchorParent = GetTreeCtrl ().GetParentItem (m_hLastMultiSelectAnchor);
			if (hParent != nullptr && hParent == hAnchorParent) {
				m_selectedMeshItems.Delete_All ();
				bool inRange = false;
				for (HTREEITEM hSib = GetTreeCtrl ().GetChildItem (hParent); hSib; hSib = GetTreeCtrl ().GetNextSiblingItem (hSib)) {
					bool isEndpoint = (hSib == hItem || hSib == m_hLastMultiSelectAnchor);
					if (isEndpoint)
						inRange = !inRange;
					if (inRange || isEndpoint)
						m_selectedMeshItems.Add (hSib);
					if (isEndpoint && !inRange)
						break;
				}
				GetTreeCtrl ().SelectItem (hItem);
				return;
			}
		}

		if ((uFlags & TVHT_ONITEM) && (nFlags & MK_CONTROL)) {
			// Ctrl+click on the item label: add/remove from multi-select set.
			bool found = false;
			for (int i = 0; i < m_selectedMeshItems.Count (); ++i) {
				if (m_selectedMeshItems[i] == hItem) {
					m_selectedMeshItems.Delete (i);
					found = true;
					break;
				}
			}
			if (!found)
				m_selectedMeshItems.Add (hItem);
			m_hLastMultiSelectAnchor = hItem;
			// Highlight the item visually but don't change tree selection.
			GetTreeCtrl ().SelectItem (hItem);
			return;
		}
	}

	// Any non-Ctrl/Shift click clears the multi-select set and updates the anchor.
	if (!(nFlags & MK_CONTROL) && !(nFlags & MK_SHIFT)) {
		m_selectedMeshItems.Delete_All ();
		m_hLastMultiSelectAnchor = (hItem != nullptr && Is_Mesh_Sub_Object (hItem) && (uFlags & TVHT_ONITEM)) ? hItem : nullptr;
	}

	CTreeView::OnLButtonDown (nFlags, point);
}


////////////////////////////////////////////////////////////////////////////
//
//  OnToggleMeshVis
//
void
CDataTreeView::OnToggleMeshVis (void)
{
	HTREEITEM hItem = m_hRightClickItem;
	m_hRightClickItem = nullptr;
	if (hItem != nullptr)
		Toggle_Mesh_Visibility (hItem);
}


////////////////////////////////////////////////////////////////////////////
//
//  Set_Mesh_Visibility
//
// TheSuperHackers @feature Tria 23/04/2026 Set the hidden state of a single sub-mesh on the
// currently displayed model and update its tree checkbox. Does not recurse into children.
//
void
CDataTreeView::Set_Mesh_Visibility (HTREEITEM hItem, bool visible)
{
	AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (hItem);
	if (asset_info == nullptr || asset_info->Get_Type () != TypeMesh)
		return;

	CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
	if (pdoc == nullptr)
		return;

	RenderObjClass *pDisplayed = pdoc->GetDisplayedObject ();
	if (pDisplayed == nullptr)
		return;

	RenderObjClass *pSubObj = pDisplayed->Get_Sub_Object_By_Name (asset_info->Get_Name (), nullptr);
	if (pSubObj == nullptr)
		return;

	pSubObj->Set_Hidden (!visible);
	pSubObj->Release_Ref ();

	GetTreeCtrl ().SetItemState (hItem, INDEXTOSTATEIMAGEMASK (visible ? 2 : 1), TVIS_STATEIMAGEMASK);
}


////////////////////////////////////////////////////////////////////////////
//
//  Set_Mesh_Visibility_Recursive
//
// TheSuperHackers @feature Tria 23/04/2026 Set visibility on hItem and all its tree descendants.
//
void
CDataTreeView::Set_Mesh_Visibility_Recursive (HTREEITEM hItem, bool visible)
{
	Set_Mesh_Visibility (hItem, visible);
	for (HTREEITEM hChild = GetTreeCtrl ().GetChildItem (hItem); hChild; hChild = GetTreeCtrl ().GetNextSiblingItem (hChild))
		Set_Mesh_Visibility_Recursive (hChild, visible);
}


////////////////////////////////////////////////////////////////////////////
//
//  Toggle_Mesh_Visibility
//
// TheSuperHackers @feature Tria 18/04/2026 Toggle hidden state of a sub-mesh on the currently
// displayed model. If Ctrl is held, adds hItem to the multi-select set. Clicking on a parent
// item cascades the toggle to all children.
//
void
CDataTreeView::Toggle_Mesh_Visibility (HTREEITEM hItem)
{
	// Determine target visibility from the current checkbox state of this item.
	UINT state = GetTreeCtrl ().GetItemState (hItem, TVIS_STATEIMAGEMASK);
	int stateIdx = (state >> 12) & 0xF;
	bool currentlyVisible = (stateIdx == 2);
	bool newVisible = !currentlyVisible;

	// If this item has children (group node), cascade to all descendants.
	if (GetTreeCtrl ().GetChildItem (hItem) != nullptr) {
		Set_Mesh_Visibility_Recursive (hItem, newVisible);
		return;
	}

	// Apply to all items in the multi-select set (if any), plus this item.
	bool appliedToSet = false;
	if (m_selectedMeshItems.Count () > 0) {
		for (int i = 0; i < m_selectedMeshItems.Count (); ++i) {
			HTREEITEM hSel = m_selectedMeshItems[i];
			if (hSel != nullptr)
				Set_Mesh_Visibility_Recursive (hSel, newVisible);
		}
		appliedToSet = true;
	}

	if (!appliedToSet)
		Set_Mesh_Visibility (hItem, newVisible);
}


////////////////////////////////////////////////////////////////////////////
//
//  OnShowBones
//
// TheSuperHackers @feature Tria 18/04/2026 Toggle bone overlay rendering in the viewport.
//
void
CDataTreeView::OnShowBones (void)
{
	CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
	if (pdoc != nullptr) {
		pdoc->SetShowBones (!pdoc->GetShowBones ());
		::AfxGetApp ()->WriteProfileInt ("Config", "ShowBones", pdoc->GetShowBones () ? 1 : 0);
	}
}



////////////////////////////////////////////////////////////////////////////
//
//  OnExportInfo
//
// TheSuperHackers @feature Tria 19/04/2026 Export texture and mesh info for a hierarchy object
// to a text file. Lists all sub-meshes and textures used by each mesh.
//
void
CDataTreeView::OnExportInfo (void)
{
	if (m_hRightClickItem == nullptr)
		return;

	AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (m_hRightClickItem);
	if (asset_info == nullptr)
		return;

	const char *objectName = asset_info->Get_Name ();

	// Create the render object to inspect
	RenderObjClass *robj = WW3DAssetManager::Get_Instance ()->Create_Render_Obj (objectName);
	if (robj == nullptr) {
		::AfxMessageBox ("Could not create render object for inspection.");
		return;
	}

	// Ask the user where to save
	CString defaultName (objectName);
	defaultName += "_info.txt";
	CFileDialog dlg (FALSE, "txt", defaultName,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER,
		"Text Files (*.txt)|*.txt|All Files (*.*)|*.*||", this);
	dlg.m_ofn.lpstrTitle = "Export Info";

	if (dlg.DoModal () != IDOK) {
		robj->Release_Ref ();
		return;
	}

	CString filePath = dlg.GetPathName ();
	FILE *fp = fopen ((LPCTSTR)filePath, "w");
	if (fp == nullptr) {
		::AfxMessageBox ("Could not open file for writing.");
		robj->Release_Ref ();
		return;
	}

	fprintf (fp, "Object: %s\n", objectName);
	fprintf (fp, "========================================\n\n");

	// Collect all meshes and their textures
	int numSub = robj->Get_Num_Sub_Objects ();
	fprintf (fp, "Meshes (%d):\n", numSub);
	fprintf (fp, "----------------------------------------\n");

	HashTemplateClass<StringClass, int> allTextures;

	for (int s = 0; s < numSub; ++s) {
		RenderObjClass *sub = robj->Get_Sub_Object (s);
		if (sub == nullptr)
			continue;

		fprintf (fp, "  [%d] %s\n", s, sub->Get_Name ());

		// Get textures for this sub-object
		MaterialInfoClass *matinfo = sub->Get_Material_Info ();
		if (matinfo != nullptr) {
			for (int t = 0; t < matinfo->Texture_Count (); ++t) {
				TextureClass *tex = matinfo->Peek_Texture (t);
				if (tex != nullptr) {
					const char *texName = tex->Get_Texture_Name ();
					fprintf (fp, "        Texture: %s\n", texName);
					StringClass key (texName);
					if (!allTextures.Exists (key))
						allTextures.Insert (key, 1);
				}
			}
			REF_PTR_RELEASE (matinfo);
		}

		sub->Release_Ref ();
	}

	// If the object itself is a mesh (no sub-objects), check it directly
	if (numSub == 0) {
		fprintf (fp, "  (single mesh) %s\n", objectName);
		MaterialInfoClass *matinfo = robj->Get_Material_Info ();
		if (matinfo != nullptr) {
			for (int t = 0; t < matinfo->Texture_Count (); ++t) {
				TextureClass *tex = matinfo->Peek_Texture (t);
				if (tex != nullptr) {
					const char *texName = tex->Get_Texture_Name ();
					fprintf (fp, "        Texture: %s\n", texName);
					StringClass key (texName);
					if (!allTextures.Exists (key))
						allTextures.Insert (key, 1);
				}
			}
			REF_PTR_RELEASE (matinfo);
		}
	}

	// Summary of all unique textures
	fprintf (fp, "\n========================================\n");
	fprintf (fp, "All Textures:\n");
	fprintf (fp, "----------------------------------------\n");

	HashTemplateIterator<StringClass, int> texIter (allTextures);
	for (texIter.First (); !texIter.Is_Done (); texIter.Next ()) {
		fprintf (fp, "  %s\n", (const char *)texIter.Peek_Key ());
	}

	fprintf (fp, "\n");
	fclose (fp);

	robj->Release_Ref ();

	CString msg;
	msg.Format ("Info exported to:\n%s", (LPCTSTR)filePath);
	::AfxMessageBox (msg, MB_ICONINFORMATION);
}


////////////////////////////////////////////////////////////////////////////
//
//  OnDumpW3DInfo
//
// TheSuperHackers @feature Tria 23/04/2026 Show a popup with the hierarchy object's parent name,
// sub-object names, and bone names.
//
void
CDataTreeView::OnDumpW3DInfo (void)
{
	if (m_hRightClickItem == nullptr)
		return;

	AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (m_hRightClickItem);
	if (asset_info == nullptr)
		return;

	const char *objectName = asset_info->Get_Name ();

	RenderObjClass *robj = WW3DAssetManager::Get_Instance ()->Create_Render_Obj (objectName);
	if (robj == nullptr) {
		::AfxMessageBox ("Could not create render object for inspection.");
		return;
	}

	// Mirror the tree view hierarchy logic:
	// Iterate HTree pivots, mix sub-objects and bones nested by parent-child relationship,
	// sort children alphabetically at each level, same as TVI_SORT in the tree.
	const HTreeClass *htree = robj->Get_HTree ();
	int numPivots = (htree != nullptr) ? htree->Num_Pivots () : 0;
	int numSubs = robj->Get_Num_Sub_Objects ();

	// Build short-name -> bool mesh presence map (strip "HIERARCHY." prefix)
	HashTemplateClass<StringClass, int> subObjNames;
	for (int s = 0; s < numSubs; ++s) {
		RenderObjClass *sub = robj->Get_Sub_Object (s);
		if (sub == nullptr)
			continue;
		const char *fullName = sub->Get_Name ();
		const char *dot = ::strchr (fullName, '.');
		StringClass shortName (dot != nullptr ? dot + 1 : fullName);
		subObjNames.Insert (shortName, 1);
		sub->Release_Ref ();
	}

	// Build per-pivot child lists (pivot index -> sorted list of child pivot indices)
	// pivot 0 is the root; children of 0 are top-level items
	DynamicVectorClass<DynamicVectorClass<int>> children;
	children.Resize (numPivots);
	for (int p = 1; p < numPivots; ++p) {
		const char *name = htree->Get_Bone_Name (p);
		if (name == nullptr || name[0] == '\0')
			continue;
		int parent = htree->Get_Parent_Index (p);
		if (parent < 0 || parent >= numPivots)
			parent = 0;
		children[parent].Add (p);
	}

	// Sort each child list alphabetically by pivot name
	for (int p = 0; p < numPivots; ++p) {
		DynamicVectorClass<int> &list = children[p];
		// Insertion sort (lists are small)
		for (int i = 1; i < list.Count (); ++i) {
			int key = list[i];
			const char *keyName = htree->Get_Bone_Name (key);
			int j = i - 1;
			while (j >= 0 && ::_stricmp (htree->Get_Bone_Name (list[j]), keyName) > 0) {
				list[j + 1] = list[j];
				--j;
			}
			list[j + 1] = key;
		}
	}

	// Recursively append a sub-object and any children (bones or nested meshes) under it.
	// Bones that are direct children of pivot 0 are excluded here; they go to the Bones section.
	struct PivotPrinter {
		static void Print (CString &out, const HTreeClass *tree,
		                   const DynamicVectorClass<DynamicVectorClass<int>> &ch,
		                   const HashTemplateClass<StringClass, int> &meshNames,
		                   int pivotIdx, int depth)
		{
			const char *name = tree->Get_Bone_Name (pivotIdx);
			if (name == nullptr || name[0] == '\0')
				return;
			CString indent;
			for (int i = 0; i < depth; ++i)
				indent += "  ";
			bool isMesh = meshNames.Exists (StringClass (name));
			out.AppendFormat ("%s%s [%s]\n", (LPCSTR)indent, name, isMesh ? "object" : "bone");
			const DynamicVectorClass<int> &list = ch[pivotIdx];
			for (int i = 0; i < list.Count (); ++i)
				Print (out, tree, ch, meshNames, list[i], depth + 1);
		}
	};

	CString text;
	text.Format ("%s\n", objectName);

	if (numPivots > 1) {
		// Sub-Objects section: root-level pivots that have mesh geometry, plus their children
		CString subObjLines;
		int subObjCount = 0;
		CString rootBoneLines;
		int rootBoneCount = 0;
		const DynamicVectorClass<int> &roots = children[0];
		for (int i = 0; i < roots.Count (); ++i) {
			int p = roots[i];
			const char *name = htree->Get_Bone_Name (p);
			if (name == nullptr || name[0] == '\0')
				continue;
			if (subObjNames.Exists (StringClass (name))) {
				subObjLines.AppendFormat ("  %s [object]\n", name);
				// Print children indented under this sub-object
				const DynamicVectorClass<int> &ch = children[p];
				for (int c = 0; c < ch.Count (); ++c)
					PivotPrinter::Print (subObjLines, htree, children, subObjNames, ch[c], 2);
				++subObjCount;
			} else {
				rootBoneLines.AppendFormat ("  %s\n", name);
				++rootBoneCount;
			}
		}
		text.AppendFormat ("\nSub-Objects (%d):\n", subObjCount);
		text += subObjLines;
		text.AppendFormat ("\nBones (%d):\n", rootBoneCount);
		text += rootBoneLines;
	} else {
		// No HTree — list sub-objects alphabetically, no bones section
		DynamicVectorClass<CString> names;
		for (int s = 0; s < numSubs; ++s) {
			RenderObjClass *sub = robj->Get_Sub_Object (s);
			if (sub == nullptr)
				continue;
			const char *fullName = sub->Get_Name ();
			const char *dot = ::strchr (fullName, '.');
			names.Add (CString (dot != nullptr ? dot + 1 : fullName));
			sub->Release_Ref ();
		}
		for (int i = 1; i < names.Count (); ++i) {
			CString key = names[i];
			int j = i - 1;
			while (j >= 0 && names[j].CompareNoCase (key) > 0) {
				names[j + 1] = names[j];
				--j;
			}
			names[j + 1] = key;
		}
		text.AppendFormat ("\nSub-Objects (%d):\n", names.Count ());
		for (int i = 0; i < names.Count (); ++i)
			text.AppendFormat ("  %s [object]\n", (LPCSTR)names[i]);
	}

	robj->Release_Ref ();

	// Show info with a Copy button using TaskDialog
	TASKDIALOG_BUTTON buttons[] = { { 100, L"Copy to Clipboard" }, { IDOK, L"OK" } };
	CStringW textW (text);
	TASKDIALOGCONFIG cfg = {};
	cfg.cbSize = sizeof (cfg);
	cfg.hwndParent = GetSafeHwnd ();
	cfg.dwFlags = 0;
	cfg.pszWindowTitle = L"W3D Info";
	cfg.pszMainIcon = TD_INFORMATION_ICON;
	cfg.pszContent = textW;
	cfg.pButtons = buttons;
	cfg.cButtons = 2;
	cfg.nDefaultButton = IDOK;
	int pressed = IDOK;
	::TaskDialogIndirect (&cfg, &pressed, nullptr, nullptr);
	if (pressed == 100) {
		if (::OpenClipboard (nullptr)) {
			::EmptyClipboard ();
			HGLOBAL hMem = ::GlobalAlloc (GMEM_MOVEABLE, (text.GetLength () + 1) * sizeof (char));
			if (hMem != nullptr) {
				char *pMem = (char *)::GlobalLock (hMem);
				::memcpy (pMem, (LPCSTR)text, text.GetLength () + 1);
				::GlobalUnlock (hMem);
				::SetClipboardData (CF_TEXT, hMem);
			}
			::CloseClipboard ();
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  OnDeleteItem
//
void
CDataTreeView::OnDeleteItem
(
	NMHDR *pNMHDR,
	LRESULT *pResult
)
{
	// Get the information object for this asset
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	AssetInfoClass *asset_info = (AssetInfoClass *)pNMTreeView->itemOld.lParam;

	// If this is a texture, then free our hold on its interface
	if (asset_info && (asset_info->Get_Type () == TypeMaterial)) {
		TextureClass *ptexture = (TextureClass *)asset_info->Get_User_Number ();
		REF_PTR_RELEASE (ptexture);
	}

	// Free the asset information object
	SAFE_DELETE (asset_info);

	// TheSuperHackers @performance Tria 25/04/2026 Removed redundant SetItemData(... 0).
	// The tree item is being destroyed by the control - setting its data sends a
	// TVM_SETITEM back into the control mid-teardown, which is pointless and adds
	// a SendMessage round-trip per item (very expensive when clearing thousands of items).
	(*pResult) = 0;
}


////////////////////////////////////////////////////////////////////////////
//
//  Free_All_Asset_Data
//
// TheSuperHackers @performance Tria 25/04/2026 Walk the tree once, release
// each AssetInfo's texture ref and free the AssetInfo, then zero the lParam.
// After this returns, OnDeleteItem becomes a no-op for every item, so the
// subsequent DeleteAllItems() call does not have to release thousands of D3D
// textures one-by-one in the middle of tree teardown - the worst part of the
// 2000-asset "New" hang.
//
////////////////////////////////////////////////////////////////////////////
void
CDataTreeView::Free_All_Asset_Data (void)
{
	CTreeCtrl &tree = GetTreeCtrl ();

	// Iterative depth-first walk over every item.
	HTREEITEM hitem = tree.GetRootItem ();
	while (hitem != nullptr) {

		AssetInfoClass *asset_info = (AssetInfoClass *)tree.GetItemData (hitem);
		if (asset_info != nullptr) {
			if (asset_info->Get_Type () == TypeMaterial) {
				TextureClass *ptexture = (TextureClass *)asset_info->Get_User_Number ();
				REF_PTR_RELEASE (ptexture);
			}
			delete asset_info;
			tree.SetItemData (hitem, 0);
		}

		// Descend, then advance to next sibling, climbing back up as needed.
		HTREEITEM hnext = tree.GetChildItem (hitem);
		if (hnext == nullptr) {
			hnext = tree.GetNextSiblingItem (hitem);
			while (hnext == nullptr) {
				hitem = tree.GetParentItem (hitem);
				if (hitem == nullptr) {
					break;
				}
				hnext = tree.GetNextSiblingItem (hitem);
			}
		}
		hitem = hnext;
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Get_Current_Asset_Info
//
AssetInfoClass *
CDataTreeView::Get_Current_Asset_Info () const
{
	AssetInfoClass *asset_info = nullptr;

	// Get the currently selected node from the tree control
	HTREEITEM htree_item = GetTreeCtrl ().GetSelectedItem ();
	if (htree_item != nullptr) {

		// Get the data associated with this item
		asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
	}

	// Return the asset information associated with the current item
	return asset_info;
}


////////////////////////////////////////////////////////////////////////////
//
//  Get_Current_Render_Obj
//
RenderObjClass *
CDataTreeView::Get_Current_Render_Obj () const
{
	RenderObjClass *prender_obj = nullptr;

	// Get the currently selected node from the tree control
	HTREEITEM htree_item = GetTreeCtrl ().GetSelectedItem ();
	if (htree_item != nullptr) {

		// Get the data associated with this item
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
		if (asset_info != nullptr) {

			// Return the render object pointer
			prender_obj = asset_info->Peek_Render_Obj ();
		}
	}

	// Return the render object pointer
	return prender_obj;
}


////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentSelectionName
//
LPCTSTR
CDataTreeView::GetCurrentSelectionName ()
{
	LPCTSTR pname = nullptr;

	// Get the currently selected node from the tree control
	HTREEITEM htree_item = GetTreeCtrl ().GetSelectedItem ();
	if (htree_item != nullptr) {

		// Get the data associated with this item
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
		if (asset_info != nullptr) {

			// Return the name of the asset to the caller
			pname = asset_info->Get_Name ();
		}
	}

	// Return the name of the selected node
	return pname;
}

////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentSelectionType
//
ASSET_TYPE
CDataTreeView::GetCurrentSelectionType ()
{
	ASSET_TYPE type = TypeUnknown;

	// Get the currently selected node from the tree control
	HTREEITEM htree_item = GetTreeCtrl ().GetSelectedItem ();
	if (htree_item != nullptr) {

		// Get the associated asset information for this node.
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
		if (asset_info != nullptr) {
			type = asset_info->Get_Type ();
		}
	}

	// Return the type of the selected node
	return type;
}

////////////////////////////////////////////////////////////////////////////
//
//  OnDblclk
//
void
CDataTreeView::OnDblclk
(
    NMHDR* pNMHDR,
    LRESULT* pResult
)
{
    // Get the main window of our app
    CMainFrame *pCMainWnd = (CMainFrame *)::AfxGetMainWnd ();
    if (pCMainWnd != nullptr)
    {
        // Display the properties for the currently selected object
        //pCMainWnd->ShowObjectProperties ();
    }

	(*pResult) = 0;
}


////////////////////////////////////////////////////////////////////////////
//
//  OnOpenInMaterialViewer
//
// TheSuperHackers @feature Opens the right-clicked asset in the W3D Material
// Viewer: the preview shows the render object, and the material panel is fed
// from the asset's source .w3d file (resolved via the source-file map).
//
void
CDataTreeView::OnOpenInMaterialViewer (void)
{
	if (m_hRightClickItem == nullptr)
		return;

	AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (m_hRightClickItem);
	if (asset_info == nullptr)
		return;

	const char *name = asset_info->Get_Name ();

	// Look up which .w3d file this asset came from
	StringClass source_file;
	CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
	if (pdoc != nullptr) {
		const HashTemplateClass<StringClass, StringClass> &sourceMap = pdoc->GetAssetSourceFileMap ();
		StringClass key (name);
		if (sourceMap.Exists (key)) {
			source_file = sourceMap.Get (key);
		}
	}

	// Mesh items pre-select themselves in the material panel
	bool is_mesh = (asset_info->Get_Type () == TypeMesh);
	CMaterialViewerFrame::ShowViewerForAsset (name, source_file, is_mesh ? name : nullptr);
}


////////////////////////////////////////////////////////////////////////////
//
//  OpenSelectionInMaterialViewer
//
// TheSuperHackers @feature Ctrl+M entry point. A mesh selected in the tree
// opens the Material Viewer pre-selected to that mesh; any other selection (or
// none) opens the whole currently displayed object instead.
//
void
CDataTreeView::OpenSelectionInMaterialViewer (void)
{
	AssetInfoClass *asset_info = Get_Current_Asset_Info ();

	// A mesh is selected: open that mesh (pre-selected in the panel), mirroring
	// the right-click "Open in Material Viewer" path.
	if (asset_info != nullptr && asset_info->Get_Type () == TypeMesh) {
		const char *name = asset_info->Get_Name ();

		StringClass source_file;
		CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
		if (pdoc != nullptr) {
			const HashTemplateClass<StringClass, StringClass> &sourceMap = pdoc->GetAssetSourceFileMap ();
			StringClass key (name);
			if (sourceMap.Exists (key)) {
				source_file = sourceMap.Get (key);
			}
		}

		CMaterialViewerFrame::ShowViewerForAsset (name, source_file, name);
		return;
	}

	// Otherwise open the whole currently displayed object. Prefer a selected
	// non-mesh asset's name; fall back to whatever the viewport is showing.
	CW3DViewDoc *pdoc = (CW3DViewDoc *)GetDocument ();
	const char *obj_name = nullptr;
	if (asset_info != nullptr) {
		obj_name = asset_info->Get_Name ();
	} else if (pdoc != nullptr && pdoc->GetDisplayedObject () != nullptr) {
		obj_name = pdoc->GetDisplayedObject ()->Get_Name ();
	}

	if (obj_name == nullptr || obj_name[0] == '\0') {
		// Nothing to show a model for: just open the viewer empty.
		CMaterialViewerFrame::ShowViewer ();
		return;
	}

	StringClass source_file;
	if (pdoc != nullptr) {
		const HashTemplateClass<StringClass, StringClass> &sourceMap = pdoc->GetAssetSourceFileMap ();
		StringClass key (obj_name);
		if (sourceMap.Exists (key)) {
			source_file = sourceMap.Get (key);
		}
	}

	// No mesh name -> the panel shows the whole object.
	CMaterialViewerFrame::ShowViewerForAsset (obj_name, source_file, nullptr);
}


////////////////////////////////////////////////////////////////////////////
//
//  Build_Render_Object_List
//
void
CDataTreeView::Build_Render_Object_List
(
	DynamicVectorClass <CString> &asset_list,
	HTREEITEM hparent
)
{
	// Loop through all the children of this node
	for (HTREEITEM htree_item = GetTreeCtrl ().GetChildItem (hparent);
		  (htree_item != nullptr);
		  htree_item = GetTreeCtrl ().GetNextSiblingItem (htree_item)) {

		// Determine if this is an asset type we want to add to the list
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
		if ((asset_info != nullptr) &&
			 (asset_info->Get_Type () != TypeAnimation) &&
			 (asset_info->Get_Type () != TypeCompressedAnimation) &&
			 (asset_info->Get_Type () != TypeMaterial))
		{
			asset_list.Add (GetTreeCtrl ().GetItemText (htree_item));
		}

		// If this item has children, then add the children recursively
		if (GetTreeCtrl ().ItemHasChildren (htree_item)) {
			Build_Render_Object_List (asset_list, htree_item);
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Create_Render_Obj_To_Display
//
////////////////////////////////////////////////////////////////////////////
RenderObjClass *
CDataTreeView::Create_Render_Obj_To_Display (HTREEITEM htree_item)
{
	// Lookup the information object associated with this asset
	RenderObjClass *render_obj = nullptr;
	AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
	if (asset_info != nullptr) {

		// Use the asset's instance if there is one, otherwise attempt to create one
		render_obj = asset_info->Get_Render_Obj ();
		if (render_obj == nullptr) {

			// If this is a texture, then create a special BMP obj from it
			if (asset_info->Get_Type () == TypeMaterial) {
				TextureClass *ptexture = (TextureClass *)asset_info->Get_User_Number ();
				if (ptexture != nullptr) {
					render_obj = new Bitmap2DObjClass (ptexture, 0.5F, 0.5F, true, false, false, true);
				}
			}

			//
			// Finally, if we aren't successful, create a new instance based on its name
			//
			if (render_obj == nullptr) {
				render_obj = WW3DAssetManager::Get_Instance()->Create_Render_Obj (asset_info->Get_Name ());
			}
		}
	}

	//
	//	Force the highest level LOD
	//
	if (	render_obj != nullptr &&
			::GetCurrentDocument ()->GetScene ()->Are_LODs_Switching () == false)
	{
		Set_Highest_LOD (render_obj);
	}

	return render_obj;
}


////////////////////////////////////////////////////////////////////////////
//
//  Refresh_Asset
//
void
CDataTreeView::Refresh_Asset
(
	LPCTSTR new_name,
	LPCTSTR old_name,
	ASSET_TYPE type
)
{
	// Params OK?
	if ((new_name != nullptr) && (old_name != nullptr)) {

		// Turn off repainting
		GetTreeCtrl ().SetRedraw (FALSE);

		// Determime where this asset should go
		HTREEITEM hparent = nullptr;
		int icon_index = 0;
		Determine_Tree_Location (type, hparent, icon_index);

		// Can we find the item we are supposed to refresh?
		HTREEITEM htree_item = FindChildItem (hparent, old_name);
		if (htree_item != nullptr) {

			// Refresh the item's text in the tree control
			GetTreeCtrl ().SetItemText (htree_item, new_name);

			// Refresh the associated asset info structure
			AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (htree_item);
			if (asset_info != nullptr) {
				asset_info->Set_Name (new_name);
			}
		} else {

			// This asset wasn't already in the tree, so add it...
			Add_Asset_To_Tree (new_name, type, true);
		}

		// Turn painting back on
		GetTreeCtrl ().SetRedraw (TRUE);

		// Force the window to be repainted
		Invalidate (FALSE);
		UpdateWindow ();
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Select_Next
//
void
CDataTreeView::Select_Next ()
{
	//
	//	Get the selected entry in the tree control
	//
	HTREEITEM hselected = GetTreeCtrl ().GetSelectedItem ();
	if (hselected != nullptr) {

		//
		//	Select the item that follows the currently selected item
		//
		HTREEITEM hitem = GetTreeCtrl ().GetNextItem (hselected, TVGN_NEXT);
		if (hitem != nullptr) {
			GetTreeCtrl ().SelectItem (hitem);
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Select_Prev
//
void
CDataTreeView::Select_Prev ()
{
	//
	//	Get the selected entry in the tree control
	//
	HTREEITEM hselected = GetTreeCtrl ().GetSelectedItem ();
	if (hselected != nullptr) {

		//
		//	Select the item that follows the currently selected item
		//
		HTREEITEM hitem = GetTreeCtrl ().GetNextItem (hselected, TVGN_PREVIOUS);
		if (hitem != nullptr) {
			GetTreeCtrl ().SelectItem (hitem);
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Reload_Lightmap_Models
//
////////////////////////////////////////////////////////////////////////////
void
CDataTreeView::Reload_Lightmap_Models ()
{
	Free_Child_Models (m_hMeshCollectionRoot);
	Free_Child_Models (m_hHierarchyRoot);
	Free_Child_Models (m_hMeshRoot);
}


////////////////////////////////////////////////////////////////////////////
//
//  Free_Child_Models
//
////////////////////////////////////////////////////////////////////////////
void
CDataTreeView::Free_Child_Models (HTREEITEM parent_item)
{
	//
	// Loop through all the children of this node
	//
	for (	HTREEITEM tree_item = GetTreeCtrl ().GetChildItem (parent_item);
			tree_item != nullptr;
			tree_item = GetTreeCtrl ().GetNextSiblingItem (tree_item))
	{
		//
		// Get the data associated with this item
		//
		AssetInfoClass *asset_info = (AssetInfoClass *)GetTreeCtrl ().GetItemData (tree_item);
		if (asset_info  != nullptr) {
			WW3DAssetManager::Get_Instance ()->Remove_Prototype (asset_info->Get_Name ());
		}

		// TheSuperHackers @feature Tria 19/04/2026 Recurse into group nodes for grouped mesh items.
		if (GetTreeCtrl ().ItemHasChildren (tree_item)) {
			Free_Child_Models (tree_item);
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Set_Highest_LOD
//
////////////////////////////////////////////////////////////////////////////
void
Set_Highest_LOD (RenderObjClass *render_obj)
{
	if (render_obj != nullptr) {
		for (int index = 0; index < render_obj->Get_Num_Sub_Objects (); index ++) {
			RenderObjClass *sub_obj = render_obj->Get_Sub_Object (index);
			if (sub_obj != nullptr) {
				Set_Highest_LOD (sub_obj);
			}
			REF_PTR_RELEASE (sub_obj);
		}

		//
		// Switcht this LOD to its highest level
		//
		if (render_obj->Class_ID () == RenderObjClass::CLASSID_HLOD) {
			((HLodClass *)render_obj)->Set_LOD_Level (((HLodClass *)render_obj)->Get_Lod_Count () - 1);
		}
	}
}


////////////////////////////////////////////////////////////////////////////
//
//  Restrict_Anims
//
////////////////////////////////////////////////////////////////////////////
void
CDataTreeView::Restrict_Anims (bool onoff)
{
	if (m_RestrictAnims != onoff) {
		m_RestrictAnims = onoff;

		//
		//	Reload the tree
		//
		GetTreeCtrl ().DeleteAllItems ();
		CreateRootNodes ();
		LoadAssetsIntoTree ();
	}
}

// TheSuperHackers @feature Themed checkbox state icons that respect dark mode.
// State 1 = unchecked (hidden), state 2 = checked (visible/shown).
// Index 0 is a reserved transparent dummy; comctl32 treats state image index 0 as
// "no image".
void CDataTreeView::RebuildStateImageList (void)
{
    const bool isDark = W3DDarkMode::IsDark ();
    const COLORREF bgColor = isDark
        ? W3DDarkMode::GetCtrlBackgroundColor ()
        : 0x00FFFFFF; // light path keeps the original opaque white
    // Pre-multiply into a 32-bit BGR (no alpha) DIB pixel.
    const DWORD bgPixel = 0xFF000000u
        | (static_cast<DWORD> (GetRValue (bgColor)) << 16)
        | (static_cast<DWORD> (GetGValue (bgColor)) << 8)
        |  static_cast<DWORD> (GetBValue (bgColor));

    HTHEME hTheme = ::OpenThemeData (GetSafeHwnd (), L"BUTTON");

    CImageList stateImageList;
    stateImageList.Create (16, 16, ILC_COLOR32, 3, 1);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = 16;
    bmi.bmiHeader.biHeight      = -16; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    auto buildOne = [&] (int themeState, int dfcsFlags) {
        void *pvBits = nullptr;
        HBITMAP hBmp = ::CreateDIBSection (nullptr, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        if (hBmp && pvBits) {
            DWORD *p = static_cast<DWORD *> (pvBits);
            for (int i = 0; i < 16 * 16; i++) p[i] = bgPixel;
            HDC hDC = ::CreateCompatibleDC (nullptr);
            HBITMAP hOld = (HBITMAP)::SelectObject (hDC, hBmp);
            RECT rc = { 1, 1, 15, 15 };
            if (hTheme)
                ::DrawThemeBackground (hTheme, hDC, BP_CHECKBOX, themeState, &rc, nullptr);
            else
                ::DrawFrameControl (hDC, &rc, DFC_BUTTON, dfcsFlags);
            ::SelectObject (hDC, hOld);
            ::DeleteDC (hDC);
            ::ImageList_Add (stateImageList.GetSafeHandle (), hBmp, nullptr);
            ::DeleteObject (hBmp);
        }
    };

    // Index 0 — reserved dummy (transparent).
    {
        void *pvBits = nullptr;
        HBITMAP hBmp = ::CreateDIBSection (nullptr, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        if (hBmp && pvBits) {
            DWORD *p = static_cast<DWORD *> (pvBits);
            for (int i = 0; i < 16 * 16; i++) p[i] = 0x00000000;
            ::ImageList_Add (stateImageList.GetSafeHandle (), hBmp, nullptr);
            ::DeleteObject (hBmp);
        }
    }

    buildOne (CBS_UNCHECKEDNORMAL, DFCS_BUTTONCHECK);
    buildOne (CBS_CHECKEDNORMAL,   DFCS_BUTTONCHECK | DFCS_CHECKED);

    if (hTheme)
        ::CloseThemeData (hTheme);

    // SetImageList returns the previous image list; let MFC clean it up via the
    // owning CImageList wrapper going out of scope.
    GetTreeCtrl ().SetImageList (&stateImageList, TVSIL_STATE);
    stateImageList.Detach ();
}

LRESULT CDataTreeView::OnW3DViewThemeChanged (WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (::IsWindow (GetSafeHwnd ()))
    {
        RebuildNormalImageList ();
        RebuildStateImageList ();
        GetTreeCtrl ().Invalidate ();
    }
    return 0;
}

// TheSuperHackers @feature Rebuilds the tree-view's TVSIL_NORMAL icon list using
// the dark-mode resource IDs when dark mode is active, or the original light icons
// otherwise. Index assignments (m_iAnimationIcon, m_iMeshIcon, etc.) are preserved
// in both paths so existing item bindings remain valid.
void CDataTreeView::RebuildNormalImageList (void)
{
    const bool isDark = W3DDarkMode::IsDark ();

    if (m_normalImageList.GetSafeHandle ())
    {
        m_normalImageList.DeleteImageList ();
    }
    m_normalImageList.Create (16, 16, ILC_COLOR32 | ILC_MASK, 12, 0);

    auto addIcon = [&] (UINT idLight, UINT idDark) -> int {
        UINT id = isDark ? idDark : idLight;
        HICON h = (HICON)::LoadImage (::AfxGetResourceHandle (), MAKEINTRESOURCE (id),
            IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        return m_normalImageList.Add (h);
    };

    m_iAnimationIcon    = addIcon (IDI_ANIMATION,                  IDI_TREE_ANIMATION_DM);
    m_iTCAnimationIcon  = addIcon (IDI_ANIMATION_COMPRESSED,       IDI_TREE_ANIMATION_COMPRESSED_DM);
    m_iADAnimationIcon  = addIcon (IDI_ANIMATION_COMPRESSED_DELTA, IDI_TREE_ANIMATION_DELTA_DM);
    m_iMeshIcon         = addIcon (IDI_MESH,                       IDI_TREE_MESH_DM);
    m_iMaterialIcon     = addIcon (IDI_MATERIAL,                   IDI_TREE_MATERIAL_DM);
    m_iLODIcon          = addIcon (IDI_LOD,                        IDI_TREE_LOD_DM);
    m_iAggregateIcon    = addIcon (IDI_HIERARCHY,                  IDI_TREE_HIERARCHY_DM);
    m_iEmitterIcon      = addIcon (IDI_HIERARCHY,                  IDI_TREE_HIERARCHY_DM);
    m_iHierarchyIcon    = addIcon (IDI_HIERARCHY,                  IDI_TREE_HIERARCHY_DM);
    m_iPrimitivesIcon   = addIcon (IDI_PRIMITIVES,                 IDI_TREE_PRIMITIVES_DM);
    m_iSoundIcon        = addIcon (IDI_SOUND,                      IDI_TREE_SOUND_DM);
    m_iBoneIcon         = addIcon (IDI_BONE,                       IDI_TREE_BONE_DM);

    GetTreeCtrl ().SetImageList (&m_normalImageList, TVSIL_NORMAL);
}

