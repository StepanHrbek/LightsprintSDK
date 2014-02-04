// --------------------------------------------------------------------------
// Scene viewer - GL canvas in main window.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVCANVAS_H
#define SVCANVAS_H

#include "Lightsprint/Ed/Ed.h"
#include "Lightsprint/GL/PluginFPS.h"
#include <GL/glew.h> // must go before wx/GLCanvas
#include "wx/GLCanvas.h"
#include "wx/joystick.h"
#include "SVApp.h"
#include "SVEntityIcons.h"
#include "SVLightmapViewer.h"
#include "SVSaveLoad.h"

namespace rr_ed
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVCanvas

	class SVCanvas: public wxGLCanvas
	{
	public:
		SVCanvas( SceneViewerStateEx& svse, class SVFrame* svframe, wxSize size);
		~SVCanvas();

		// initializes gl context and other stuff, must be called once after canvas is created and Show()n
		// (wx asserts if we do it before our parent calls Show())
		void createContextCore();
		void createContext();

		void recalculateIconSizeAndPosition();

		//! Adds scene to solver or removes it from solver. If scene is NULL, just reloads current data in solver.
		//! - scene,true = add scene, setStaticObjects, reallocateBuffersForRealtimeGI, cleanup
		//! - scene,false = remove scene, setStaticObjects, cleanup
		//! - NULL,true = setStaticObjects, reallocateBuffersForRealtimeGI, cleanup
		//! - NULL,false = cleanup
		void addOrRemoveScene(rr::RRScene* scene, bool add, bool staticObjectsAlreadyModified);

		void reallocateBuffersForRealtimeGI(bool reallocateAlsoVbuffers);

		void dropMaterialAt(rr::RRMaterial* droppedMaterial, const rr::RRObject* destinationObject, unsigned destinationTriangle);

		// set context, paint, swap, catch exceptions
		void OnPaint(wxPaintEvent& event);
		// paints to current buffer, may be called from outside to paint hires screenshot to texture
		void Paint(bool takingSshot, const wxString& extraMessage);
		// helper
		void PaintCore(bool takingSshot, const wxString& extraMessage);

		void OnSize(wxSizeEvent& event);
		void OnSizeCore(bool force);

		void OnEraseBackground(wxEraseEvent& event) {} // Do nothing, to avoid flashing.
		int FilterEvent(wxKeyEvent& event);
		void OnKeyDown(wxKeyEvent& event);
		void OnKeyUp(wxKeyEvent& event);
		void OnMouseEvent(wxMouseEvent& event);
		void OnContextMenuCreate(wxContextMenuEvent& event);
		void OnIdle(wxIdleEvent& event);


		bool                       renderEmptyFrames;
		// public only for SVFrame::selectEntity()
		rr_gl::RRDynamicSolverGL*  solver;
		EntityType                 selectedType;
		// public only for SVSceneTree::runContextMenuAction()
		rr::RRLights               lightsToBeDeletedOnExit; // list of lights owned and deleted by us
		std::vector<rr::RRScene*>  mergedScenes; // Inited empty, filled by user via menu File/Open|Merge, deleted when no longer needed.
		bool                       fireballLoadAttempted; // Public only for SVSceneTree.

	private:
		class wxGLContext*         context; // context for this canvas (we have only one canvas, so there's no need to share context yet)
		class SVFrame*             svframe;
		SceneViewerStateEx&        svs;
		int                        winWidth; // current size
		int                        winHeight; // current size
		int                        windowCoord[4]; // x,y,w,h of window when user switched to fullscreen
		float                      speedForward;
		float                      speedBack;
		float                      speedRight;
		float                      speedLeft;
		float                      speedUp;
		float                      speedDown;
		float                      speedLean;
		float                      speedY;
		bool                       exitRequested;
		int                        menuHandle;
		bool                       envToBeDeletedOnExit; // whether environment is owned and deleted by us
		SVLightmapViewer           lv; // 2d lightmap viewer
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
		rr_gl::TextureRenderer*    textureRenderer;
		rr_gl::FpsCounter          fpsCounter;

		// icons
	public:
		rr::RRVec3                 sunIconPosition; // for dirlight icons, public only for SVSceneTree
		SVEntities                 renderedIcons; // collection of icons, updated in every frame, public only for SVSceneTree
	private:
		SVEntityIcons*             entityIcons; // permanent helper, renders and intersects icons
		IconCode                   selectedTransformation;

		// skybox blending
		bool                       skyboxBlendingInProgress;
		rr::RRTime                 skyboxBlendingStartTime;


		LightingIndirect           previousLightIndirect;

		friend class SVFrame;

		DECLARE_EVENT_TABLE()
	};
 
}; // namespace

#endif
