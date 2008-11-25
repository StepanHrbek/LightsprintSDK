// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVFRAME_H
#define SVFRAME_H

#include "SVLightProperties.h"
#include "SVCanvas.h"
#include "SVApp.h"

#define DEBUG_TEXEL


namespace rr_gl
{

	class SVFrame: public wxFrame
	{
	public:
		static SVFrame *Create(SceneViewerParameters& params);
		void OnKeyDown(wxKeyEvent& event);
		void OnMenuEvent(wxCommandEvent& event);
		void OnExit(wxCommandEvent& event);

		enum
		{
			ME_RENDER_DIFFUSE = 10000, // avoid conflict with wxWidgets event numbers
			ME_RENDER_SPECULAR,
			ME_RENDER_EMISSION,
			ME_RENDER_TRANSPARENT,
			ME_RENDER_WATER,
			ME_RENDER_TEXTURES,
			ME_RENDER_TONEMAPPING,
			ME_RENDER_WIREFRAME,
			ME_RENDER_HELPERS,
			ME_MAXIMIZE,
			ME_LIGHT_DIR,
			ME_LIGHT_SPOT,
			ME_LIGHT_POINT,
			ME_LIGHT_DELETE,
			ME_LIGHT_AMBIENT,
			ME_CAMERA_GENERATE_RANDOM,
			ME_CAMERA_SPEED0_001,
			ME_CAMERA_SPEED0_01,
			ME_CAMERA_SPEED0_1,
			ME_CAMERA_SPEED0_5,
			ME_CAMERA_SPEED2,
			ME_CAMERA_SPEED10,
			ME_CAMERA_SPEED100,
			ME_CAMERA_SPEED1000,
			ME_CAMERA_SPEED10000,
			ME_CHECK_SOLVER,
			ME_CHECK_SCENE,
			ME_REALTIME_FIREBALL,
			ME_REALTIME_ARCHITECT,
			ME_REALTIME_LDM,
			ME_REALTIME_LDM_BUILD,
			ME_REALTIME_FIREBALL_BUILD10,
			ME_REALTIME_FIREBALL_BUILD100,
			ME_REALTIME_FIREBALL_BUILD1000,
			ME_REALTIME_FIREBALL_BUILD10000,
			ME_STATIC_3D,
			ME_STATIC_2D,
			ME_STATIC_BILINEAR,
			ME_STATIC_ASSIGN_VBUFS,
			ME_STATIC_ASSIGN_MAPS16,
			ME_STATIC_ASSIGN_MAPS64,
			ME_STATIC_ASSIGN_MAPS256,
			ME_STATIC_ASSIGN_MAPS1024,
			ME_STATIC_BUILD1,
			ME_STATIC_BUILD10,
			ME_STATIC_BUILD100,
			ME_STATIC_BUILD1000,
			ME_STATIC_BUILD_1OBJ,
	#ifdef DEBUG_TEXEL
			ME_STATIC_DIAGNOSE1,
			ME_STATIC_DIAGNOSE10,
			ME_STATIC_DIAGNOSE100,
			ME_STATIC_DIAGNOSE1000,
	#endif
			ME_STATIC_LOAD,
			ME_STATIC_SAVE,
			ME_STATIC_BUILD_LIGHTFIELD_2D,
			ME_STATIC_BUILD_LIGHTFIELD_3D,
			ME_SHOW_LIGHT_PROPERTIES,

			ME_HELP,

			ME_ENV_WHITE,
			ME_ENV_BLACK,
			ME_ENV_WHITE_TOP,
		};

	private:
		SVFrame(wxWindow *parent, const wxString& title, const wxPoint& pos, const wxSize& size);
		//! Updates menu. May be called before m_canvas creation, doesn't read m_canvas.
		void UpdateMenuBar(const SceneViewerState& svs);

		SVCanvas*          m_canvas;
		SVLightProperties* m_lightProperties;

		DECLARE_EVENT_TABLE()
	};
 
}; // namespace

#endif
