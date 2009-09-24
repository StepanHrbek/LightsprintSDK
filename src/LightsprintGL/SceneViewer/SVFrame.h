// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVFRAME_H
#define SVFRAME_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVCanvas.h"
#include "SVApp.h"
#include "wx/aui/aui.h"

#define DEBUG_TEXEL


namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityId - identifies entity in SceneViewer

	struct EntityId
	{
		EntityType type;
		unsigned index;

		EntityId(EntityType _type, unsigned _index)
		{
			type = _type;
			index = _index;
		}
		bool operator ==(const EntityId& a)
		{
			return type==a.type && index==a.index;
		}
	};

	enum SelectEntityAction
	{
		SEA_SELECT,
		SEA_ACTION,
		SEA_ACTION_IF_ALREADY_SELECTED,
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVFrame

	class SVFrame: public wxFrame
	{
	public:
		static SVFrame *Create(SceneViewerStateEx& svse);

		void OnMenuEvent(wxCommandEvent& event);
		void OnExit(wxCommandEvent& event);
		void OnPaneOpenClose(wxAuiManagerEvent& event);
		void OnMouseMotion(wxMouseEvent& event);

		//! Returns currently selected entity.
		EntityId getSelectedEntity() const;

		//! Fully handles clicking light/object/etc (selects it in scene tree, opens its properties etc).
		void selectEntity(EntityId entity, bool updateSceneTree, SelectEntityAction action);

		//! Handles changes in light list (updates tree content, selects different light etc).
		void updateSelection();

		enum
		{
			ME_FILE_OPEN_SCENE = 10000, // avoid conflict with wxWidgets event numbers
			ME_FILE_SAVE_SCREENSHOT,
			ME_EXIT,
			ME_CAMERA_SPEED,
			ME_CAMERA_GENERATE_RANDOM,


			ME_ENV_WHITE,
			ME_ENV_BLACK,
			ME_ENV_WHITE_TOP,
			ME_ENV_OPEN,
			ME_ENV_RELOAD, // not a menu item, just command we can call from outside. reloads env from svs.skyboxFilename

			ME_LIGHT_DIR,
			ME_LIGHT_SPOT,
			ME_LIGHT_POINT,
			ME_LIGHT_DELETE,
			ME_LIGHT_AMBIENT,


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
			ME_STATIC_BUILD,
			ME_STATIC_BUILD_1OBJ,
#ifdef DEBUG_TEXEL
			ME_STATIC_DIAGNOSE,
#endif
			ME_STATIC_BUILD_LIGHTFIELD_2D,
			ME_STATIC_BUILD_LIGHTFIELD_3D,

			ME_RENDER_DIFFUSE,
			ME_RENDER_SPECULAR,
			ME_RENDER_EMISSION,
			ME_RENDER_TRANSPARENT,
			ME_RENDER_WATER,
			ME_RENDER_TEXTURES,
			ME_RENDER_WIREFRAME,
			ME_RENDER_TONEMAPPING,
			ME_RENDER_BRIGHTNESS,
			ME_RENDER_CONTRAST,
			ME_RENDER_WATER_LEVEL,
			ME_RENDER_FPS,
			ME_RENDER_ICONS,
			ME_RENDER_HELPERS,
			ME_RENDER_LOGO,
			ME_RENDER_VIGNETTE,

			ME_WINDOW_FULLSCREEN,
			ME_WINDOW_TREE,
			ME_WINDOW_PROPERTIES,

			ME_HELP,
			ME_CHECK_SOLVER,
			ME_CHECK_SCENE,
			ME_ABOUT,
		};

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

		SceneViewerStateEx&      svs; // the only svs instance used throughout whole scene viewer
		bool                     updateMenuBarNeeded;
		wxAuiManager             m_mgr;
		SVCanvas*                m_canvas;
		class SVLightProperties* m_lightProperties;
		class SVSceneTree*       m_sceneTree;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
