// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSCENETREE_H
#define SVSCENETREE_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "wx/wx.h"
#include "wx/treectrl.h"
#include "SVApp.h"
#include "SVFrame.h"

namespace rr_gl
{

	enum ContextMenu
	{
		CM_LIGHT_SPOT,
		CM_LIGHT_POINT,
		CM_LIGHT_DIR,
		CM_LIGHT_FLASH,
		CM_LIGHT_DELETE,
	};

	class SVSceneTree : public wxTreeCtrl
	{
	public:
		SVSceneTree(SVFrame* svframe);

		//! Updates whole tree contents. If solver is NULL, lights and objects are not updated.
		void updateContent(class RRDynamicSolverGL* solver);

		//! Selects entity in tree, without changing anything outside tree.
		void selectEntityInTree(EntityId entity);

		void OnSelChanged(wxTreeEvent& event);
		void OnItemActivated(wxTreeEvent& event);

		//! Forwards all unused keys to canvas.
		void OnKeyDown(wxTreeEvent& event);
		void OnKeyUp(wxKeyEvent& event);

		//! Runs context menu action, public only for SVCanvas hotkey handling.
		void runContextMenuAction(unsigned actionCode, EntityId contextEntityId);

	private:
		wxTreeItemId entityIdToItemId(EntityId entity) const;
		EntityId itemIdToEntityId(wxTreeItemId item) const;

		SVFrame* svframe;
		SceneViewerStateEx& svs;
		unsigned callDepth;
		bool needsUpdateContent;
		wxTreeItemId lights;
		wxTreeItemId staticObjects;
		wxTreeItemId dynamicObjects;

		// context menu
		void OnContextMenuCreate(wxTreeEvent& event);
		void OnContextMenuRun(wxCommandEvent& event); // public only for SVCanvas hotkey handling

		// temporary, set when creating context menu, valid only while context menu exists
		wxTreeItemId temporaryContext;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
