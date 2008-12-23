// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr_gl
{

	struct SceneViewerParameters
	{
		rr::RRDynamicSolver* solver;
		const char* pathToShaders;
		SceneViewerState svs;
	};

}; // namespace

#endif
