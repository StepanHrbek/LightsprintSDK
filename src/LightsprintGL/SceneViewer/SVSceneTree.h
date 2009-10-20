// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSCENETREE_H
#define SVSCENETREE_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

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

		void updateContent(class RRDynamicSolverGL* solver);

		//! Selects entity in tree, without changing anything outside tree.
		void selectItem(EntityId entity);

		void OnSelChanged(wxTreeEvent& event);
		void OnItemActivated(wxTreeEvent& event);

		//! Forwards all unused keys to canvas.
		void OnKeyDown(wxTreeEvent& event);

	private:
		wxTreeItemId findItem(EntityId entity, bool& isOk) const;

		SceneViewerStateEx& svs;
		bool allowEvents;
		wxTreeItemId lights;
		wxTreeItemId objects;
		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
