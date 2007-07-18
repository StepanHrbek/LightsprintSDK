// --------------------------------------------------------------------------
// DemoEngine
// Water with reflection and waves.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef WATER_H
#define WATER_H

#include "Program.h"
#include "Texture.h"
#include "Camera.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Water

class RR_GL_API Water
{
public:
	Water(const char* pathToShaders, bool fesnel, bool boostSun);
	~Water();

	//! Prepares pipeline for rendering into reflection map.
	//! \param reflWidth
	//!  Width of reflection texture. Typically 1/2 or 1/4 of main viewport width.
	//! \param reflHeight
	//!  Height of reflection texture. Typically 1/2 or 1/4 of main viewport height.
	//! \param eye
	//!  Camera used for rendering scene. It will be modified
	//! \param altitude
	//!  Altitude of water surface in world.
	void updateReflectionInit(unsigned reflWidth, unsigned reflHeight, Camera* eye, float altitude=0);

	//! Finishes rendering into reflection map, restores camera settings.
	void updateReflectionDone();

	//! Renders water quad with reflection.
	//
	//! If you don't want simple quad, call render(0) and issue your own
	//! water primitives, reflection shader is still active and will be used.
	//! \param size
	//!  Size of water quad in center of world (-size..size).
	void render(float size);

protected:
	Texture* mirrorMap;
	Texture* mirrorDepth;
	Program* mirrorProgram;
	Camera*  eye;
	float    altitude;
	int      viewport[4];
	bool     fresnel;
};

}; // namespace

#endif
