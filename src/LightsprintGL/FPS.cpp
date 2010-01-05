// --------------------------------------------------------------------------
// FPS display.
// Copyright (C) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/FPS.h"
#include "tmpstr.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FpsCounter

unsigned FpsCounter::getFps()
{
	TIME now = GETTIME;
	while (times.size() && now-times.front()>PER_SEC) times.pop();
	times.push(GETTIME);
	unsigned fpsToRender = (unsigned)times.size();
	return fpsToRender;
}


/////////////////////////////////////////////////////////////////////////////
//
// FpsDisplay

FpsDisplay* FpsDisplay::create(const char* pathToMaps)
{
	FpsDisplay* fps = new FpsDisplay(pathToMaps);
	if (!fps->mapFps) goto err;
	for (unsigned i=0;i<10;i++)
	{
		if (!fps->mapDigit[i]) goto err;
	}
	return fps;
err:
	delete fps;
	return NULL;
}

FpsDisplay::FpsDisplay(const char* pathToMaps)
{
	mapFps = rr::RRBuffer::load(tmpstr("%stxt-fps.png",pathToMaps));
	for (unsigned i=0;i<10;i++)
	{
		mapDigit[i] = rr::RRBuffer::load(tmpstr("%stxt-%d.png",pathToMaps,i));
	}
}

void FpsDisplay::render(rr_gl::TextureRenderer* textureRenderer, unsigned fpsToRender, int winWidth, int winHeight)
{
	if (textureRenderer && textureRenderer->render2dBegin(NULL))
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		char fpsstr[10];
		sprintf(fpsstr,"%d",fpsToRender);
		float wpix = 1.f/winWidth;
		float hpix = 1.f/winHeight;
		float x = 1-(mapFps->getWidth()+5+mapDigit[0]->getWidth()*3)*wpix;
		float y = 0;
		textureRenderer->render2dQuad(rr_gl::getTexture(mapFps),x,y,mapFps->getWidth()*wpix,mapFps->getHeight()*hpix);
		x += (mapFps->getWidth()+5)*wpix;
		for (char* c=fpsstr;*c;c++)
		{
			rr::RRBuffer* digit = mapDigit[*c-'0'];
			textureRenderer->render2dQuad(rr_gl::getTexture(digit),x,y+(mapFps->getHeight()-digit->getHeight())*hpix,digit->getWidth()*wpix,digit->getHeight()*hpix);
			x += (digit->getWidth()-6)*wpix;
		}
		textureRenderer->render2dEnd();
		glDisable(GL_BLEND);
	}
}

FpsDisplay::~FpsDisplay()
{
	for (unsigned i=0;i<10;i++) delete mapDigit[i];
	delete mapFps;
}

}; // namespace
