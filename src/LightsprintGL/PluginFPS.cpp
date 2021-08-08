// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// FPS counter and renderer.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginFPS.h"
#include "Lightsprint/GL/PreserveState.h"
#include <cstdio> // sprintf
#include <vector>
#include <algorithm>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FpsCounter

void FpsCounter::addFrame()
{
	rr::RRTime now;
	while (times.size() && now.secondsSince(times.front())>1) times.pop_front();
	times.push_back(now);
}

unsigned FpsCounter::getFps()
{
	return (unsigned)times.size();
}


/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeFPS

class PluginRuntimeFPS : public PluginRuntime
{
	rr::RRBuffer* mapDigit[10];
	rr::RRBuffer* mapFps;
	std::vector<float> durations;

public:

	PluginRuntimeFPS(const PluginCreateRuntimeParams& params)
	{
		mapFps = rr::RRBuffer::load(rr::RRString(0,L"%lstxt-fps.png",params.pathToMaps.w_str()));
		for (unsigned i=0;i<10;i++)
			mapDigit[i] = rr::RRBuffer::load(rr::RRString(0,L"%lstxt-%d.png",params.pathToMaps.w_str(),i));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsFPS& pp = *dynamic_cast<const PluginParamsFPS*>(&_pp);
		
		if (!mapFps) return;
		for (unsigned i=0;i<10;i++)
			if (!mapDigit[i]) return;

		PreserveFlag p0(GL_DEPTH_TEST,false);
		if (_renderer.getTextureRenderer() && _renderer.getTextureRenderer()->render2dBegin(nullptr))
		{
			// render fps number
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			char fpsstr[10];
			sprintf(fpsstr,"%d",pp.fpsCounter.getFps());
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

			// render percentile graph
			if (pp.percentileGraph && pp.fpsCounter.times.size()>1)
			{
				durations.resize(pp.fpsCounter.times.size()-1);
				std::deque<rr::RRTime>::const_iterator time1 = pp.fpsCounter.times.begin();
				for (unsigned i=0;i<pp.fpsCounter.times.size()-1;i++)
				{
					rr::RRTime time0 = *time1;
					++time1;
					durations[i] = time1->secondsSince(time0);
				}
				std::sort(durations.begin(),durations.end());
				for (unsigned i=0;i<100;i++)
				{
					float duration = durations[durations.size()*i/100];
					float durationMax = durations[durations.size()*99/100]*1.05f;
					_renderer.getTextureRenderer()->render2dQuad(rr_gl::getTexture(mapDigit[0]),i/100.f,duration/durationMax,0.01f,0.01f);
				}
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

PluginRuntime* PluginParamsFPS::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeFPS(params);
}

}; // namespace
