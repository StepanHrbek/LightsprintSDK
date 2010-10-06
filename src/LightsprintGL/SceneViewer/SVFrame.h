// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVFRAME_H
#define SVFRAME_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVCanvas.h"
#include "wx/aui/aui.h"

#define DEBUG_TEXEL


namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVFrame

	class SVFrame: public wxFrame
	{
	public:
		static SVFrame *Create(SceneViewerStateEx& svse);

		void OnMenuEvent(wxCommandEvent& event);
		void OnMenuEventCore(wxCommandEvent& event);
		void OnExit(wxCommandEvent& event);

		void OnPaneOpenClose(wxAuiManagerEvent& event);
		//! Cleanup after OnPaneOpenClose. Called from SVCanvas::OnPaint because it's so difficult to get event AFTER pane close.
		void AfterPaneOpenClose();

		//! Returns currently selected entity.
		EntityId getSelectedEntity() const;

		//! Fully handles clicking light/object/etc (selects it in scene tree, updates property panel with its properties).
		//! Panels are updated even if hidden, at least ctrl-<number> handler expects it.
		void selectEntityInTreeAndUpdatePanel(EntityId entity, SelectEntityAction action);

		//! Ensures that svs.selectedXxxIndex are in range.
		//! Updates all panels, according to svs.selectedXxxIndex.
		void updateAllPanels();

		//! Shortcut for subwindows so that they don't have to include SVSceneTree+SVCanvas.
		void updateSceneTree();

		//! Must be called before every insert/delete/select/deselect.
		//! Such operations may delete and recreate properties in panels.
		//! Deleting property with uncommited changes is not just data loss, it sometimes crashes wx.
		void commitPropertyChanges();

		//! Updates sun (first dirlight) direction, called when user changes time or location.
		void simulateSun();

		void userPreferencesGatherFromWx();
		void userPreferencesApplyToWx();

		enum
		{
			ME_FILE_OPEN_SCENE = 10000, // avoid conflict with wxWidgets event numbers
			ME_FILE_MERGE_SCENE,
			ME_FILE_SAVE_SCENE,
			ME_FILE_SAVE_SCENE_AS,
			ME_FILE_SAVE_SCREENSHOT,
			ME_FILE_SAVE_ENHANCED_SCREENSHOT,
			ME_EXIT,
			ME_VIEW_RANDOM,
			ME_VIEW_TOP,
			ME_VIEW_BOTTOM,
			ME_VIEW_LEFT,
			ME_VIEW_RIGHT,
			ME_VIEW_FRONT,
			ME_VIEW_BACK,
			ME_ENV_WHITE,
			ME_ENV_BLACK,
			ME_ENV_WHITE_TOP,
			ME_ENV_OPEN,
			ME_ENV_RELOAD, // not a menu item, just command we can call from outside. reloads env from svs.skyboxFilename

			ME_LIGHT_DIR,
			ME_LIGHT_SPOT,
			ME_LIGHT_POINT,
			ME_LIGHT_FLASH,
			ME_LIGHT_DELETE,


			ME_LIGHTING_DIRECT_REALTIME,
			ME_LIGHTING_DIRECT_STATIC,
			ME_LIGHTING_DIRECT_NONE,
			ME_LIGHTING_INDIRECT_FIREBALL_LDM,
			ME_LIGHTING_INDIRECT_FIREBALL,
			ME_LIGHTING_INDIRECT_ARCHITECT,
			ME_LIGHTING_INDIRECT_STATIC,
			ME_LIGHTING_INDIRECT_CONST,
			ME_LIGHTING_INDIRECT_NONE,
			ME_REALTIME_LDM_BUILD,
			ME_REALTIME_FIREBALL_BUILD,

			ME_STATIC_3D,
			ME_STATIC_2D,
			ME_STATIC_BILINEAR,
			ME_STATIC_BUILD_UNWRAP,
			ME_STATIC_BUILD,
			ME_STATIC_BUILD_1OBJ,
#ifdef DEBUG_TEXEL
			ME_STATIC_DIAGNOSE,
#endif
			ME_STATIC_BUILD_LIGHTFIELD_2D,
			ME_STATIC_BUILD_LIGHTFIELD_3D,

			ME_WINDOW_FULLSCREEN,
			ME_WINDOW_TREE,
			ME_WINDOW_USER_PROPERTIES,
			ME_WINDOW_SCENE_PROPERTIES,
			ME_WINDOW_LIGHT_PROPERTIES,
			ME_WINDOW_OBJECT_PROPERTIES,
			ME_WINDOW_MATERIAL_PROPERTIES,
			ME_WINDOW_LAYOUT1,
			ME_WINDOW_LAYOUT2,
			ME_WINDOW_LAYOUT3,
			ME_WINDOW_RESIZE,

			ME_HELP,
			ME_SDK_HELP,
			ME_SUPPORT,
			ME_LIGHTSPRINT,
			ME_CHECK_SOLVER,
			ME_CHECK_SCENE,
			ME_ABOUT,
		};

		SVCanvas*                    m_canvas; // public only for SVSceneTree, SVComment
		class SVSceneTree*           m_sceneTree; // public only for SVComment
		class SVLightProperties*     m_lightProperties; // public only for SVCanvas
		class SVObjectProperties*    m_objectProperties; // public only for SVCanvas
		class SVMaterialProperties*  m_materialProperties; // public only for SVCanvas
		SceneViewerStateEx&          svs; // the only svs instance used throughout whole scene viewer. public only for SVProperties
		UserPreferences              userPreferences; // public only for SVUserProperties
	private:
		//! Creates empty frame.
		SVFrame(wxWindow *parent, const wxString& title, const wxPoint& pos, const wxSize& size, SceneViewerStateEx& svse);
		virtual ~SVFrame();

		//! Updates window title bar.
		void UpdateTitle();

		//! Updates everything in frame according to svs (deletes and recreates everything).
		//! May be called repeatedly.
		void UpdateEverything();

		//! Updates menu according to svs (doesn't read canvas). May be called repeatedly.
		void UpdateMenuBar();

		bool                     updateMenuBarNeeded;
		wxAuiManager             m_mgr;
		class SVSceneProperties* m_sceneProperties;
		class SVUserProperties*  m_userProperties;

		DECLARE_EVENT_TABLE()
	};


}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
