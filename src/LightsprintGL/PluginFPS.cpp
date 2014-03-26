// --------------------------------------------------------------------------
// FPS counter and renderer.
// Copyright (C) 2006-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginFPS.h"
#include "Lightsprint/GL/PreserveState.h"
#include <cstdio> // sprintf

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
// PluginRuntimeFPS

class PluginRuntimeFPS : public PluginRuntime
{
	rr::RRBuffer* mapDigit[10];
	rr::RRBuffer* mapFps;

public:

	PluginRuntimeFPS(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
		mapFps = rr::RRBuffer::load(rr::RRString(0,L"%lstxt-fps.png",pathToMaps.w_str()));
		for (unsigned i=0;i<10;i++)
			mapDigit[i] = rr::RRBuffer::load(rr::RRString(0,L"%lstxt-%d.png",pathToMaps.w_str(),i));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsFPS& pp = *dynamic_cast<const PluginParamsFPS*>(&_pp);
		
		if (!mapFps) return;
		for (unsigned i=0;i<10;i++)
			if (!mapDigit[i]) return;

		PreserveFlag p0(GL_DEPTH_TEST,false);
		if (_renderer.getTextureRenderer() && _renderer.getTextureRenderer()->render2dBegin(NULL,1))
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			char fpsstr[10];
			sprintf(fpsstr,"%d",pp.fpsToRender);
			float wpix = 1.f/_sp.viewport[2];
			float hpix = 1.f/_sp.viewport[3];
			float x = 1-(mapFps->getWidth()+5+mapDigit[0]->getWidth()*3)*wpix;
			float y = 0;
			_renderer.getTextureRenderer()->render2dQuad(rr_gl::getTexture(mapFps),x,y,mapFps->getWidth()*wpix,mapFps->getHeight()*hpix);
			x += (mapFps->getWidth()+5)*wpix;
			for (char* c=fpsstr;*c;c++)
			{
				rr::RRBuffer* digit = mapDigit[*c-'0'];
				_renderer.getTextureRenderer()->render2dQuad(rr_gl::getTexture(digit),x,y+(mapFps->getHeight()-digit->getHeight())*hpix,digit->getWidth()*wpix,digit->getHeight()*hpix);
				x += (digit->getWidth()-6)*wpix;
			}
			_renderer.getTextureRenderer()->render2dEnd();
			glDisable(GL_BLEND);
		}
	}

	virtual ~PluginRuntimeFPS()
	{
		for (unsigned i=0;i<10;i++) delete mapDigit[i];
		delete mapFps;
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsFPS

PluginRuntime* PluginParamsFPS::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeFPS(pathToShaders, pathToMaps);
}

}; // namespace