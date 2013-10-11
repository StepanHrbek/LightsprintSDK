// --------------------------------------------------------------------------
// FPS display.
// Copyright (C) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/FPS.h"
#include "PreserveState.h"
#include "tmpstr.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FpsCounter

unsigned FpsCounter::getFps()
{
	rr::RRTime now;
	while (times.size() && now.secondsSince(times.front())>1) times.pop();
	times.push(now);
	unsigned fpsToRender = (unsigned)times.size();
	return fpsToRender;
}


/////////////////////////////////////////////////////////////////////////////
//
// FpsDisplay

FpsDisplay* FpsDisplay::create(const rr::RRString& pathToMaps)
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

FpsDisplay::FpsDisplay(const rr::RRString& pathToMaps)
{
	mapFps = rr::RRBuffer::load(rr::RRString(0,L"%stxt-fps.png",pathToMaps.w_str()));
	for (unsigned i=0;i<10;i++)
	{
		mapDigit[i] = rr::RRBuffer::load(rr::RRString(0,L"%stxt-%d.png",pathToMaps.w_str(),i));
	}
}

void FpsDisplay::render(rr_gl::TextureRenderer* textureRenderer, unsigned fpsToRender, int winWidth, int winHeight)
{
	PreserveFlag p0(GL_DEPTH_TEST,false);
	if (textureRenderer && textureRenderer->render2dBegin(NULL,1))
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
