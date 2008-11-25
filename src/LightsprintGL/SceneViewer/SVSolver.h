// --------------------------------------------------------------------------
// Scene viewer - solver for GI lighting.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVSOLVER_H
#define SVSOLVER_H

#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/UberProgramSetup.h"

namespace rr_gl
{

	class SVSolver : public RRDynamicSolverGL
	{
	public:
		SVSolver(const char* _pathToShaders, SceneViewerState& _svs);
		~SVSolver();

		void resetRenderCache();
		virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight);
		void dirtyLights();

	protected:
		char* pathToShaders;
		SceneViewerState& svs;
		RendererOfScene* rendererOfScene;
	};

}; // namespace

#endif
