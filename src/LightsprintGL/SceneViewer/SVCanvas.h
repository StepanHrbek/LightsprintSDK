// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVCANVAS_H
#define SVCANVAS_H

#include <GL/glew.h> // must go before wx/GLCanvas
#include "wx/GLCanvas.h"
#include "wx/joystick.h"
#include "SVApp.h"
#include "SVLightProperties.h"
#include "SVLightmapViewer.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/Water.h"

namespace rr_gl
{

	class SVCanvas: public wxGLCanvas
	{
	public:
		SVCanvas( SceneViewerParameters& params, class SVFrame* parent, SVLightProperties** parentsLightProps);
		~SVCanvas();

		// initializes gl context and other stuff, must be called once after canvas is created and Show()n
		// (wx asserts if we do it before our parent calls Show())
		void createContext(SceneViewerParameters& params);

		void OnPaint(wxPaintEvent& event);
		void OnSize(wxSizeEvent& event);
		void OnEraseBackground(wxEraseEvent& event) {} // Do nothing, to avoid flashing.
		void OnKeyDown(wxKeyEvent& event);
		void OnKeyUp(wxKeyEvent& event);
		void OnMouseEvent(wxMouseEvent& event);
		void OnIdle(wxIdleEvent& event);
		void OnEnterWindow(wxMouseEvent& event);


	private:
		class wxGLContext*         context; // context for this canvas (we have only one canvas, so there's no need to share context yet)
		class SVFrame*             parent;
		SceneViewerState&          svs;
		const class rr::RRDynamicSolver* originalSolver;
		class SVSolver*            solver;
		enum SelectionType {ST_CAMERA, ST_LIGHT, ST_OBJECT};
		SelectionType              selectedType;
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
		bool                       ourEnv; // whether environment is owned by us
		rr::RRLights               lightsToBeDeletedOnExit; // list of lights owned by us
		SVLightmapViewer*          lv; // 2d lightmap viewer
		unsigned                   centerObject; // object in the middle of screen
		unsigned                   centerTexel; // texel in the middle of screen
		unsigned                   centerTriangle; // triangle in the middle of screen, multiObjPostImport
		rr::RRRay*                 ray; // all users use this ray to prevent allocations in every frame
		rr::RRCollisionHandler*    collisionHandler; // all users use this collision handler to prevent allocations in every frame
		bool                       fontInited;

		// all we need for testing lightfield
		rr::RRLightField*          lightField;
		GLUquadricObj*             lightFieldQuadric;
		rr::RRObjectIllumination*  lightFieldObjectIllumination;


		SVLightProperties**        lightProperties;

		friend class SVFrame;

		DECLARE_EVENT_TABLE()
	};
 
}; // namespace

#endif
