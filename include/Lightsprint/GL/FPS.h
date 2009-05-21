//---------------------------------------------------------------------------
//! \file FPS.h
//! \brief LightsprintGL | FPS counter.
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
//! All rights reserved
//---------------------------------------------------------------------------

#ifndef FPS_H
#define FPS_H

#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/Timer.h"
#include <queue>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FpsCounter

class RR_GL_API FpsCounter
{
public:
	unsigned getFps();

protected:
	std::queue<TIME> times;
};


/////////////////////////////////////////////////////////////////////////////
//
// FpsDisplay

class RR_GL_API FpsDisplay
{
public:
	static FpsDisplay* create(const char* pathToMaps);
	void render(rr_gl::TextureRenderer* textureRenderer, unsigned fpsToRender, int winWidth, int winHeight);
	~FpsDisplay();

protected:
	FpsDisplay(const char* pathToMaps);
	rr::RRBuffer* mapDigit[10];
	rr::RRBuffer* mapFps;
};

}; // namespace

#endif
