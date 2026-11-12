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

#pragma once

// DataTreeView.h : header file
//

#include "AfxCView.h"
#include "AssetTypes.h"
#include "WWLib/Vector.h"

// Forward declarations
class RenderObjClass;
class AssetInfoClass;


/////////////////////////////////////////////////////////////////////////////
//
// CDataTreeView view
//
class CDataTreeView : public CTreeView
{
protected:
	CDataTreeView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CDataTreeView)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDataTreeView)
	public:
	virtual void OnInitialUpdate() override;
	protected:
	virtual void OnDraw(CDC* pDC) override;      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CDataTreeView() override;
#ifdef RTS_DEBUG
	virtual void AssertValid() const override;
	virtual void Dump(CDumpContext& dc) const override;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CDataTreeView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnToggleMeshVis();
	afx_msg void OnShowBones();
	afx_msg void OnExportInfo();
	afx_msg void OnDumpW3DInfo();
	// TheSuperHackers @feature Open the right-clicked asset in the W3D Material Viewer.
	afx_msg void OnOpenInMaterialViewer();
	// TheSuperHackers @feature Rebuild themed check-state image list on dark/light switch.
	afx_msg LRESULT OnW3DViewThemeChanged(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	// Rebuilds the TVSIL_STATE checkbox image list using the current theme.
	void RebuildStateImageList();

	// Rebuilds the TVSIL_NORMAL tree-node icon list using the current theme.
	// Indices into the list (m_iAnimationIcon, m_iMeshIcon, etc.) stay stable.
	void RebuildNormalImageList();

	public:

		/////////////////////////////////////////////////////////////////////
		//	Public methods
		/////////////////////////////////////////////////////////////////////

		//
		//	Asset insertion methods
		//
		bool					Add_Asset_To_Tree (LPCTSTR name, ASSET_TYPE type, bool bselect);
		void					LoadAssetsIntoTree ();
		void					Refresh_Asset (LPCTSTR new_name, LPCTSTR old_name, ASSET_TYPE type);

		//
		//	Animation insertion methods
		//
		void					LoadAnimationsIntoTree ();
		void					LoadAnimationsIntoTree (HTREEITEM hItem);

	  bool					Are_Anims_Restricted () const			{ return m_RestrictAnims; }
	  void					Restrict_Anims (bool onoff);

		//
		//	Texture insertion methods
		//
		void					Load_Materials_Into_Tree ();
		void					Load_Bones_Into_Tree ();
		void					Toggle_Mesh_Visibility (HTREEITEM hItem);

		//
		//	Display methods
		//
		void					Display_Asset (HTREEITEM htree_item = nullptr);
		void					Select_Next ();
		void					Select_Prev ();
		void					Reload_Lightmap_Models ();

		//
		// Information methods
		//
		RenderObjClass *	Get_Current_Render_Obj () const;
		AssetInfoClass *	Get_Current_Asset_Info () const;

		// Opens the Material Viewer for the current tree selection: a selected
		// mesh opens pre-selected to that mesh; any other (or no) selection opens
		// the whole currently displayed object. Used by the Ctrl+M hotkey.
		void					OpenSelectionInMaterialViewer ();
		LPCTSTR				GetCurrentSelectionName ();
		ASSET_TYPE			GetCurrentSelectionType ();
		HTREEITEM			FindChildItem (HTREEITEM hParentItem, LPCTSTR pszChildItemName);
		HTREEITEM			FindChildItem (HTREEITEM hParentItem, RenderObjClass *prender_obj);
		// Searches every asset root (Hierarchies, Meshes, LODs, etc.) for an item
		// whose AssetInfo name matches. Used by Reload-Assets restore in MainFrm.cpp.
		HTREEITEM			Find_Asset_Item_By_Name (LPCTSTR pszName);
		HTREEITEM			FindFirstChildItemBasedOnHierarchyName (HTREEITEM hParentItem, LPCTSTR pszHierarchyName);
		HTREEITEM			FindSiblingItemBasedOnHierarchyName (HTREEITEM hCurrentItem, LPCTSTR pszHierarchyName);
		void					Build_Render_Object_List (DynamicVectorClass <CString> &asset_list, HTREEITEM hparent = TVI_ROOT);

		//
		//	Initialization methods
		//
		void					CreateRootNodes ();
		void					Rebuild_State_Images ();

		// TheSuperHackers @performance Tria 25/04/2026 Bulk-release all AssetInfo data
		// and texture refs in one pass, then zero each item's lParam so the subsequent
		// DeleteAllItems() does not fire per-item TVN_DELETEITEM work.
		void					Free_All_Asset_Data ();

	protected:

		///////////////////////////////////////////////////////////////////////
		//	Protected methods
		///////////////////////////////////////////////////////////////////////
		ASSET_TYPE			Determine_Tree_Location (RenderObjClass &render_obj, HTREEITEM &hroot, int &icon_index);
		void					Determine_Tree_Location (ASSET_TYPE type, HTREEITEM &hroot, int &icon_index);
		RenderObjClass *	Create_Render_Obj_To_Display (HTREEITEM htree_item);
		void					Add_Emitters_To_Menu (HMENU hmenu, RenderObjClass &render_obj);
		void					Free_Child_Models (HTREEITEM parent_item);
		bool					Is_Mesh_Sub_Object (HTREEITEM hItem) const;
		void					Set_Mesh_Visibility (HTREEITEM hItem, bool visible);
		void					Set_Mesh_Visibility_Recursive (HTREEITEM hItem, bool visible);

	private:

		///////////////////////////////////////////////////////
		//
		//	Private member data
		//
		HTREEITEM	m_hMaterialsRoot;
		HTREEITEM	m_hMeshRoot;
		HTREEITEM	m_hAggregateRoot;
		HTREEITEM	m_hLODRoot;
		HTREEITEM	m_hMeshCollectionRoot;
		HTREEITEM	m_hEmitterRoot;
		HTREEITEM	m_hPrimitivesRoot;
		HTREEITEM	m_hHierarchyRoot;
		HTREEITEM	m_hSoundRoot;
		// TheSuperHackers @feature Owns the TVSIL_NORMAL image list so we can swap
		// between dark and light icon variants on theme change.
		CImageList	m_normalImageList;
		int			m_iAnimationIcon;
		int			m_iTCAnimationIcon;
		int			m_iADAnimationIcon;
		int			m_iMeshIcon;
		int			m_iMaterialIcon;
		int			m_iLODIcon;
		int			m_iEmitterIcon;
		int			m_iPrimitivesIcon;
		int			m_iAggregateIcon;
		int			m_iHierarchyIcon;
		int			m_iSoundIcon;
		int			m_iBoneIcon;
		HTREEITEM	m_hRightClickItem;
		bool			m_RestrictAnims;
		// TheSuperHackers @feature Tria 23/04/2026 Multi-select set for sub-object visibility toggling.
		DynamicVectorClass<HTREEITEM>	m_selectedMeshItems;
		HTREEITEM							m_hLastMultiSelectAnchor;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.
