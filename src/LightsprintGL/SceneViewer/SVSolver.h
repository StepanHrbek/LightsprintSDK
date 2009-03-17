// --------------------------------------------------------------------------
// Scene viewer - solver for GI lighting.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSOLVER_H
#define SVSOLVER_H

#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "SVApp.h"

namespace rr_gl
{

	class SVSolver : public RRDynamicSolverGL
	{
	public:
		SVSolver(SceneViewerStateEx& svs);
		~SVSolver();

		void resetRenderCache();
		virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight);
		void dirtyLights();

	protected:
		SceneViewerStateEx& svs;
		RendererOfScene* rendererOfScene;
	};

}; // namespace

#endif
