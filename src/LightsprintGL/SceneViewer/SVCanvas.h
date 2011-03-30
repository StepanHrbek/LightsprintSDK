// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVCANVAS_H
#define SVCANVAS_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/FPS.h"
#include <GL/glew.h> // must go before wx/GLCanvas
#include "wx/GLCanvas.h"
#include "wx/joystick.h"
#include "SVApp.h"
#include "SVEntity.h"
#include "SVLightmapViewer.h"
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
		void createContextCore();
		void createContext();

		//! Adds scene to solver or removes it from solver.
		//! \param add
		//!  True = add, false = remove.
		void addOrRemoveScene(rr::RRScene* scene, bool add);

		void reallocateBuffersForRealtimeGI(bool reallocateAlsoVbuffers);

		// set context, paint, swap, catch exceptions
		void OnPaint(wxPaintEvent& event);
		// paints to current buffer, may be called from outside to paint hires screenshot to texture
		void Paint(bool takingSshot);
		// helper
		void PaintCore(bool takingSshot);

		void OnSize(wxSizeEvent& event);
		void OnSizeCore();

		void OnEraseBackground(wxEraseEvent& event) {} // Do nothing, to avoid flashing.
		void OnKeyDown(wxKeyEvent& event);
		void OnKeyUp(wxKeyEvent& event);
		void OnMouseEvent(wxMouseEvent& event);
		void OnIdle(wxIdleEvent& event);
		void OnEnterWindow(wxMouseEvent& event);


		bool                       renderEmptyFrames;
		// public only for SVFrame::selectEntity()
		rr_gl::RRDynamicSolverGL*  solver;
		EntityType                 selectedType;
		// public only for SVSceneTree::runContextMenuAction()
		rr::RRLights               lightsToBeDeletedOnExit; // list of lights owned and deleted by us
		std::vector<rr::RRScene*>  mergedScenes; // Inited empty, filled by user via menu File/Open|Merge, deleted when no longer needed.
	private:
		class wxGLContext*         context; // context for this canvas (we have only one canvas, so there's no need to share context yet)
		class SVFrame*             parent;
		SceneViewerStateEx&        svs;
		int                        winWidth; // current size
		int                        winHeight; // current size
		int                        windowCoord[4]; // x,y,w,h of window when user switched to fullscreen
		class Water*               water;
		class ToneMapping*         toneMapping;
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
		SVLightmapViewer*          lv; // 2d lightmap viewer
		rr::RRVec2                 mousePositionInWindow; // -1,-1 top left corner, 0,0 center, 1,1 bottom right corner (set by OnMouseEvent)
		unsigned                   centerObject; // object pointed by mouse
		unsigned                   centerTexel; // texel pointed by mouse
		unsigned                   centerTriangle; // triangle pointed by mouse, multiObjPostImport
		rr::RRRay*                 ray; // all users use this ray to prevent allocations in every frame
		rr::RRCollisionHandler*    collisionHandler; // all users use this collision handler to prevent allocations in every frame
		bool                       fontInited;
		bool                       fullyCreated; // true only after complete constructor and createContext

		// help
		bool                       helpLoadAttempted;
		rr::RRBuffer*              helpImage;

		// bloom
		bool                       bloomLoadAttempted;
		class Bloom*               bloom;

		// lens flare
		bool                       lensFlareLoadAttempted;
		class LensFlare*           lensFlare;

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

		// skybox blending
		double                     timeWhenSkyboxBlendingStarted;


		friend class SVFrame;

		DECLARE_EVENT_TABLE()
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
