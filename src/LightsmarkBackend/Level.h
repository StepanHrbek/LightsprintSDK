#ifndef LEVEL_H
#define LEVEL_H

#include "AnimationEditor.h"
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/IO/IO.h"

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
	rr_gl::RRSolverGL* solver;

	unsigned saveIllumination(const char* path);
	unsigned loadIllumination(const char* path);
};

// arbitrary unique numbers
enum
{
	LAYER_LIGHTMAPS,
	LAYER_ENVIRONMENT,
	LAYER_LDM,
};

#endif
