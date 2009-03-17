// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/RRScene.h"

namespace rr_gl
{

	//! Extended SceneViewerState.
	struct SceneViewerStateEx : public SceneViewerState
	{
		//! Initial scene to be displayed, never deleted, never NULLed even when no longer displayed (because solver->aborting is still used).
		rr::RRDynamicSolver* initialInputSolver;
		//! Inited to NULL, created by user via menu File/Open, deleted when no longer needed.
		rr::RRScene* manuallyOpenedScene;
		//! Initial and never changing path to shaders.
		const char* pathToShaders;
	};

}; // namespace

#endif
