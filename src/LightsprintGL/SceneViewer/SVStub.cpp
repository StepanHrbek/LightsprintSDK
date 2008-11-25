// --------------------------------------------------------------------------
// Scene viewer - stub for platforms where it is disabled.
// Copyright (C) Stepan Hrbek, Lightsprint, 2008, All rights reserved
// --------------------------------------------------------------------------

#include "SVApp.h"

namespace rr_gl
{

void sceneViewer(rr::RRDynamicSolver* _solver, bool _createWindow, const char* _pathToShaders, SceneViewerState* _svs)
{
	rr::RRReporter::report(rr::WARN,"Scene Viewer was disabled for this platform for lack of demand, please let us know if you need it.\n");
}
 
}; // namespace

