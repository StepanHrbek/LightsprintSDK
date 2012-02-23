// --------------------------------------------------------------------------
// Scene viewer - scene tree window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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
		CM_ROOT_SCALE = 1, // OSX does not support menu item 0, so we start by 1
		CM_ROOT_AXES,

		CM_ENV_CUSTOM,
		CM_ENV_WHITE,
		CM_ENV_BLACK,

		CM_LIGHT_SPOT,
		CM_LIGHT_POINT,
		CM_LIGHT_DIR,
		CM_LIGHT_FLASH,

		CM_DELETE,

		CM_OBJECTS_UNWRAP,
		CM_OBJECTS_BUILD_LMAPS,
		CM_OBJECTS_BUILD_AMBIENT_MAPS,
		CM_OBJECTS_BUILD_LDMS,
		CM_OBJECTS_SMOOTH,
		CM_OBJECTS_MERGE,
		CM_OBJECTS_TANGENTS,
		CM_OBJECTS_DELETE_DIALOG,
		CM_OBJECT_INSPECT_UNWRAP,

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

		// what is selected
		enum ManipulatedEntityIds
		{
			MEI_CAMERA,
			MEI_SELECTED,
			MEI_AUTO, // selected or camera, see SVSceneTree::getEntityIds()
		};
		const EntityIds& getEntityIds(SVSceneTree::ManipulatedEntityIds preference) const;
		rr::RRVec3 getCenterOf(const EntityIds& entityIds) const;

		//! Runs context menu action, public only for SVCanvas hotkey handling.
		//
		//! EntityIds is not reference because selectedEntityIds may change several times from this function
		//! and we don't want our parameter to change.
		void runContextMenuAction(unsigned actionCode, const EntityIds contextEntityIds);

		// context menu, public only for canvas context menu
		void OnContextMenuCreate(wxTreeEvent& event);
		void OnContextMenuRun(wxCommandEvent& event);
		wxTreeItemId entityIdToItemId(EntityId entity) const;
		EntityId itemIdToEntityId(wxTreeItemId item) const;
		unsigned manipulateEntity(EntityId entity, const rr::RRMatrix3x4& transformation, bool rollChangeAllowed);
		unsigned manipulateEntities(const EntityIds& entityIds, const rr::RRMatrix3x4& transformation, bool rollChangeAllowed);

	private:
		void updateSelectedEntityIds();
		EntityIds selectedEntityIds; // always up to date, with root of lights replaced by all lights, root of objects replaced by all objects
		bool selectedObjectRoot; // always up to date, true if staticObjects or dynamicObjects root is selected

		SVFrame* svframe;
		SceneViewerStateEx& svs;
		unsigned callDepth;
		bool needsUpdateContent;
		wxTreeItemId root;
		wxTreeItemId lights;
		wxTreeItemId staticObjects;
		wxTreeItemId dynamicObjects;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
