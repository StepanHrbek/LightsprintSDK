#ifndef LEVEL_H
#define LEVEL_H

#include "AnimationEditor.h"
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

	LevelSetup* setup;
	AnimationEditor* animationEditor;
	rr::RRScene* scene;
	rr_gl::RRDynamicSolverGL* solver;

	unsigned saveIllumination(const char* path);
	unsigned loadIllumination(const char* path);

	unsigned getLDMLayer() {return 1;}
};

// arbitrary unique numbers
enum
{
	LAYER_LIGHTMAPS,
	LAYER_ENVIRONMENT,
};

#endif
