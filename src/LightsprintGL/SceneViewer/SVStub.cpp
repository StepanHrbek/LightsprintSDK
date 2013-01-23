// --------------------------------------------------------------------------
// Scene viewer - stub for platforms where it is disabled.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"

namespace rr_gl
{

void sceneViewer(rr::RRDynamicSolver* _inputSolver, const char* _inputFilename, const char* _skyboxFilename, const char* _pathToShaders, SceneViewerState* _svs, bool _releaseResources)
{
	rr::RRReporter::report(rr::WARN,"Scene Viewer was disabled at compile time, please let us know if you need it.\n");
}

}; // namespace

#endif // !SUPPORT_SCENEVIEWER
