// --------------------------------------------------------------------------
// Scene viewer - stub for platforms where it is disabled.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SUPPORT_SCENEVIEWER

#include "Lightsprint/Ed/Ed.h"

namespace rr_ed
{

void sceneViewer(rr::RRDynamicSolver* _inputSolver, const rr::RRString& _inputFilename, const rr::RRString& _skyboxFilename, const rr::RRString& _pathToData, SceneViewerState* _svs, bool _releaseResources)
{
	rr::RRReporter::report(rr::WARN,"Scene Viewer was disabled at compile time, please let us know if you need it.\n");
}

}; // namespace

#endif // !SUPPORT_SCENEVIEWER