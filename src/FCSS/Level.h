#ifndef LEVEL_H
#define LEVEL_H

#include "AnimationEditor.h"
#include "Autopilot.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/IO/ImportScene.h"

/////////////////////////////////////////////////////////////////////////////
//
// Level

class Level
{
public:
	Level(LevelSetup* levelSetup, rr::RRBuffer* skyMap, bool supportEditor);
	~Level();

	Autopilot pilot;
	AnimationEditor* animationEditor;
	rr::RRScene* scene;
	rr_gl::RRDynamicSolverGL* solver;
	class Bugs* bugs;
	rr_gl::RendererOfScene* rendererOfScene;

	unsigned saveIllumination(const char* path);
	unsigned loadIllumination(const char* path);

	unsigned getLDMLayer() {return 1;}
};

#endif
