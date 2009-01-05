// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
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
