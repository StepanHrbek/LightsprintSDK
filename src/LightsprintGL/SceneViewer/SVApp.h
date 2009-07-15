// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/RRScene.h"

namespace rr_gl
{

	//! Extended SceneViewerState.
	struct SceneViewerStateEx : public SceneViewerState
	{
		//! Initial scene to be displayed, never deleted, never NULLed even when no longer displayed (because solver->aborting is still used).
		rr::RRDynamicSolver* initialInputSolver;
		//! Initial and never changing path to shaders. Not freed.
		const char* pathToShaders;
		//! Current scene filename, e.g. scene.dae or scene.3ds. It is free()d in destructor.
		char* sceneFilename;
		//! Current skybox filename, e.g. skybox.hdr or skybox_ft.tga. It is free()d in destructor.
		//! To specify Quake-style cube map, use name of any one of 6 images (Quake uses suffixes ft,bk,up,dn,rt,lf).
		char* skyboxFilename;
		bool releaseResources;

		SceneViewerStateEx()
		{
			initialInputSolver = NULL;
			pathToShaders = NULL;
			sceneFilename = NULL;
			skyboxFilename = NULL;
			releaseResources = true;
		}
		~SceneViewerStateEx()
		{
			free(sceneFilename);
		}
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
