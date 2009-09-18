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
#include "SVLightmapViewer.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/FPS.h"

namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityType - identifies entity type in SceneViewer

	enum EntityType
	{
		ST_CAMERA,
		ST_LIGHT,
		ST_OBJECT,
	};

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
		class SVSolver*            solver;
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
		unsigned                   centerObject; // object in the middle of screen
		unsigned                   centerTexel; // texel in the middle of screen
		unsigned                   centerTriangle; // triangle in the middle of screen, multiObjPostImport
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


		friend class SVFrame;

		DECLARE_EVENT_TABLE()
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
