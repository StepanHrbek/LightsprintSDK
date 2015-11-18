// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - main window with menu and AUI based panels.
// --------------------------------------------------------------------------

#ifndef SVFRAME_H
#define SVFRAME_H

#define BOOST_NO_CXX11_SCOPED_ENUMS // prevents OSX/clang linker error undefined symbol boost::filesystem::detail::copy_file

#include "Lightsprint/Ed/Ed.h"
#include "SVCanvas.h"
#include "SVDialogs.h"
#include "wx/aui/aui.h"
#include "wx/propgrid/propgrid.h"
#ifdef SUPPORT_OCULUS
	#include "OVR.h"
#endif

//#define DEBUG_TEXEL ...not working, will be revived later when needed
//#define SV_LIGHTFIELD

// both should return the same correct value
// pane->IsShown() would be wrong, as explained in http://trac.wxwidgets.org/ticket/11537
//#define IS_SHOWN(pane) m_mgr.GetPane(pane).IsShown()
#define IS_SHOWN(pane) ((pane) && (pane)->IsShownOnScreen())


namespace rr_ed
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVFrame

	class SVFrame: public wxFrame
	{
	public:
		static SVFrame *Create(SceneViewerStateEx& svse);

		//! Updates menu according to svs (doesn't read canvas). May be called repeatedly.
		void UpdateMenuBar();

		void OnMenuEvent(wxCommandEvent& event);
		void OnMenuEventCore(unsigned eventCode);
		void OnMenuEventCore2(unsigned eventCode);

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

		void enableTooltips(bool enable);

		void userPreferencesGatherFromWx();
		void userPreferencesApplyToWx();

		bool oculusActive(); // true if active (not necessarily fullscreen)

		//! Loads and returns scene with normalized units and up axis.
		rr::RRScene* loadScene(const wxString& filename, float units, unsigned upAxis, bool popup);

		//! Asks for saveable scene filename, modifies it only on success.
		bool chooseSceneFilename(wxString fileSelectorCaption, wxString& selectedFilename);

		//! Gathers scene data from solver and saves it to given filename (.rr3).
		bool saveScene(wxString sceneFilename);

		enum EventSource
		{
			ES_KEYBOARD_MID_MOVEMENT,
			ES_KEYBOARD_END,
			ES_MOUSE_MID_MOVEMENT,
			ES_MOUSE_END,
			ES_MENU,
			ES_CONTEXT_MENU,
			ES_RIFT,
			ES_PROPERTY,
			ES_MISC
		};
		//! Called on any state change.
		void OnAnyChange(EventSource eventSource, const wxPGProperty* property, const wxEvent* event, unsigned menuEventCode);
		//! Increased each time state changes.
		unsigned stateVersion;


		enum
		{
			ME_FIRST = 10000, // avoid conflict with wxWidgets event numbers, and also with CM_XXX
			ME_FILE_OPEN_SCENE = ME_FIRST,
			ME_FILE_MERGE_SCENE,
			ME_FILE_SAVE_SCENE,
			ME_FILE_SAVE_SCENE_AS,
			ME_FILE_SAVE_SCREENSHOT,
			ME_FILE_SAVE_SCREENSHOT_ORIGINAL,
			ME_FILE_SAVE_SCREENSHOT_ENHANCED,
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

			ME_LIGHTING_INDIRECT_FIREBALL,
			ME_LIGHTING_INDIRECT_ARCHITECT,
			ME_REALTIME_FIREBALL_BUILD,

#ifdef DEBUG_TEXEL
			ME_STATIC_DIAGNOSE,
#endif
#ifdef SV_LIGHTFIELD
			ME_STATIC_BUILD_LIGHTFIELD_2D,
			ME_STATIC_BUILD_LIGHTFIELD_3D,
#endif

			ME_MATERIAL_LOAD,
			ME_MATERIAL_SAVE,
			ME_MATERIAL_SAVE_ALL,

			ME_WINDOW_FULLSCREEN_META, // SV: identical to non-META. RL: toggles two predefined workspaces
			ME_WINDOW_FULLSCREEN, // toggles fullscreen in current workspace
			ME_WINDOW_TREE,
			ME_WINDOW_USER_PROPERTIES,
			ME_WINDOW_SCENE_PROPERTIES,
			ME_WINDOW_GI_PROPERTIES,
			ME_WINDOW_LIGHT_PROPERTIES,
			ME_WINDOW_OBJECT_PROPERTIES,
			ME_WINDOW_MATERIAL_PROPERTIES,
			ME_WINDOW_LOG,
			ME_WINDOW_LAYOUT_RESET,
			ME_WINDOW_LAYOUT1,
			ME_WINDOW_LAYOUT2,
			ME_WINDOW_LAYOUT3,
			ME_WINDOW_RESIZE,
#ifdef _WIN32
			ME_WINDOW_SMALLLUXGPU,
#endif

			ME_HELP,
			ME_SDK_HELP,
			ME_SUPPORT,
			ME_LIGHTSPRINT,
			ME_CHECK_SOLVER,
			ME_CHECK_SCENE,
			ME_RENDER_DDI,
			ME_ABOUT,
		};

		CanvasWindow*                m_canvasWindow;
		SVCanvas*                    m_canvas; // public only for SVSceneTree, SVComment
		class SVSceneTree*           m_sceneTree; // public only for SVComment
		class SVLightProperties*     m_lightProperties; // public only for SVCanvas
		class SVObjectProperties*    m_objectProperties; // public only for SVCanvas
		class SVMaterialProperties*  m_materialProperties; // public only for SVCanvas
		class SVLog*                 m_log; // public only for SVCanvas
		SceneViewerStateEx&          svs; // the only svs instance used throughout whole scene viewer. public only for SVProperties
		UserPreferences              userPreferences; // public only for SVUserProperties
		rr::RRFileLocator*           textureLocator;
		SmoothDlg                    smoothDlg;
		DeleteDlg                    deleteDlg;
		BakeDlg                      bakeDlg;
		UnwrapDlg                    unwrapDlg;

		rr::RRTime                   lastInteractionTime; // OnAnyChange() called, e.g. because item from drop-down list was selected (requires several clicks)

#ifdef SUPPORT_OCULUS
		ovrHmd                       oculusHMD;
#endif

	private:
		//! Creates empty frame.
		SVFrame(wxWindow *parent, const wxString& title, const wxPoint& pos, const wxSize& size, SceneViewerStateEx& svse);
		virtual ~SVFrame();

		//! Updates window title bar.
		void UpdateTitle();

		//! Updates everything in frame according to svs (deletes and recreates everything).
		//! May be called repeatedly.
		void UpdateEverything();
		
		void TogglePane(wxWindow* window);

		void saveBakedLayers();
		void saveScreenshot(rr::RRBuffer* sshot);

		bool                     fullyInited; // true after constructor
		bool                     updateMenuBarNeeded;
		wxAuiManager             m_mgr;
		class SVSceneProperties* m_sceneProperties;
		class SVGIProperties*    m_giProperties;
		class SVUserProperties*  m_userProperties;

		ImportDlg                importDlg;

		DECLARE_EVENT_TABLE()
	};


// naming convention for lightmaps and ldm. final name is prefix+objectnumber+postfix
#define LAYER_PREFIX  RR_WX2RR(svs.sceneFilename.BeforeLast('.')+"_precalculated/")
	#define LMAP_POSTFIX (svs.lightmapFloats?"exr":"png") // not lightmap.png, because BuildLightmaps tool also does not insert "lightmap."
#define AMBIENT_POSTFIX (svs.lightmapFloats?"indirect.exr":"indirect.png")
#define LDM_POSTFIX "ldm.png"
#define ENV_POSTFIX "cube.rrbuffer"

}; // namespace

#endif
