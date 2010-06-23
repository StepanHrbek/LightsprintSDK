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

	class SVSceneTree : public wxTreeCtrl
	{
	public:
		SVSceneTree(wxWindow* parent, SceneViewerStateEx& svse);

		// If solver is NULL, lights and objects are not updated.
		void updateContent(class RRDynamicSolverGL* solver);

		//! Selects entity in tree, without changing anything outside tree.
		void selectItem(EntityId entity);

		void OnSelChanged(wxTreeEvent& event);
		void OnItemActivated(wxTreeEvent& event);

		//! Forwards all unused keys to canvas.
		void OnKeyDown(wxTreeEvent& event);
		void OnKeyUp(wxKeyEvent& event);

	private:
		wxTreeItemId entityIdToItemId(EntityId entity) const;
		EntityId itemIdToEntityId(wxTreeItemId item) const;

		SceneViewerStateEx& svs;
		bool allowEvents;
		wxTreeItemId lights;
		wxTreeItemId staticObjects;
		wxTreeItemId dynamicObjects;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
