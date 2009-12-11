// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVCANVAS_H
#define SVCANVAS_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include <GL/glew.h> // must go before wx/GLCanvas
#include "wx/GLCanvas.h"
#include "wx/joystick.h"
#include "SVApp.h"
#include "SVEntity.h"
#include "SVLightmapViewer.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/FPS.h"
#include "SVSaveLoad.h"

namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVCanvas

	class SVCanvas: public wxGLCanvas
	{
	public:
		SVCanvas( SceneViewerStateEx& svse, class SVFrame* parent, wxSize size);
		~SVCanvas();

		// initializes gl context and other stuff, must be called once after canvas is created and Show()n
		// (wx asserts if we do it before our parent calls Show())
		void createContext();

		void OnPaint(wxPaintEvent& event);
		void OnSize(wxSizeEvent& event);
		void OnEraseBackground(wxEraseEvent& event) {} // Do nothing, to avoid flashing.
		void OnKeyDown(wxKeyEvent& event);
		void OnKeyUp(wxKeyEvent& event);
		void OnMouseEvent(wxMouseEvent& event);
		void OnIdle(wxIdleEvent& event);
		void OnEnterWindow(wxMouseEvent& event);


		// public only for SVFrame::selectEntity()
		rr_gl::RRDynamicSolverGL*  solver;
		EntityType                 selectedType;
	private:
		class wxGLContext*         context; // context for this canvas (we have only one canvas, so there's no need to share context yet)
		class SVFrame*             parent;
		SceneViewerStateEx&        svs;
		rr::RRScene*               manuallyOpenedScene; // Inited to NULL, created by user via menu File/Open, deleted when no longer needed.
		int                        winWidth; // current size
		int                        winHeight; // current size
		int                        windowCoord[4]; // x,y,w,h of window when user switched to fullscreen
		Water*                     water;
		ToneMapping*               toneMapping;
		bool                       fireballLoadAttempted;
		float                      speedForward;
		float                      speedBack;
		float                      speedRight;
		float                      speedLeft;
		float                      speedUp;
		float                      speedDown;
		float                      speedLean;
		bool                       exitRequested;
		int                        menuHandle;
		bool                       envToBeDeletedOnExit; // whether environment is owned and deleted by us
		rr::RRLights               lightsToBeDeletedOnExit; // list of lights owned and deleted by us
		SVLightmapViewer*          lv; // 2d lightmap viewer
		rr::RRVec2                 mousePositionInWindow; // -1,-1 top left corner, 0,0 center, 1,1 bottom right corner (set by OnMouseEvent)
		unsigned                   centerObject; // object pointed by mouse
		unsigned                   centerTexel; // texel pointed by mouse
		unsigned                   centerTriangle; // triangle pointed by mouse, multiObjPostImport
		rr::RRRay*                 ray; // all users use this ray to prevent allocations in every frame
		rr::RRCollisionHandler*    collisionHandler; // all users use this collision handler to prevent allocations in every frame
		bool                       fontInited;

		// help
		bool                       helpLoadAttempted;
		rr::RRBuffer*              helpImage;

		// vignette
		bool                       vignetteLoadAttempted;
		rr::RRBuffer*              vignetteImage;

		// logo
		bool                       logoLoadAttempted;
		rr::RRBuffer*              logoImage;

		// lightfield
		rr::RRLightField*          lightField;
		GLUquadricObj*             lightFieldQuadric;
		rr::RRObjectIllumination*  lightFieldObjectIllumination;

		// FPS
		TextureRenderer*           textureRenderer; // used also by help
		bool                       fpsLoadAttempted;
		FpsCounter                 fpsCounter;
		FpsDisplay*                fpsDisplay;

		// icons
		rr::RRVec3                 sunIconPosition; // for dirlight icons
		float                      iconSize; // for icons
		class SVEntityIcons*       entityIcons;


		friend class SVFrame;

		DECLARE_EVENT_TABLE()
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
