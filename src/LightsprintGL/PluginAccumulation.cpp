// --------------------------------------------------------------------------
// Accumulates frames, outputs average.
// Copyright (C) 2010-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginAccumulation.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

#ifdef RR_GL_ES2

PluginRuntime* PluginParamsAccumulation::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return NULL;
}

#else //!RR_GL_ES2

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeAccumulation

class PluginRuntimeAccumulation : public PluginRuntime
{
	Texture* colorMap;
	Texture* depthMap;
	Texture* accumulationMap;
	unsigned numAccumulatedFrames;
	float accumulatedBrightness;

public:

	PluginRuntimeAccumulation(const PluginCreateRuntimeParams& params)
	{
		reentrancy = 1;
		colorMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false);
		depthMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false);
		accumulationMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBF,true,RR_GHOST_BUFFER),false,false);
		numAccumulatedFrames = 0;
		accumulatedBrightness = 0;
	}

	virtual ~PluginRuntimeAccumulation()
	{
		delete accumulationMap;
		delete depthMap;
		delete colorMap;
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		if (!colorMap || !depthMap || !accumulationMap)
		{
			_renderer.render(_pp.next,_sp);
			return;
		}

		const PluginParamsAccumulation& pp = *dynamic_cast<const PluginParamsAccumulation*>(&_pp);

		bool srgbCorrect = _sp.srgbCorrect; // can be changed to true, correct accumulation is slightly more expensive, but works even without sRGB textures (i.e. in GL ES 2.0)

		if (pp.restart)
		{
			numAccumulatedFrames = 0;
			accumulatedBrightness = 0;
		}

		// adjust map sizes to match render target size
		unsigned w = _sp.viewport[2];
		unsigned h = _sp.viewport[3];
		if (w!=colorMap->getBuffer()->getWidth() || h!=colorMap->getBuffer()->getHeight())
		{
			numAccumulatedFrames = 0;
			accumulatedBrightness = 0;

			colorMap->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
			colorMap->reset(false,false,_sp.srgbCorrect);

			depthMap->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_DEPTH,false,RR_GHOST_BUFFER);
			depthMap->reset(false,false,false);

			accumulationMap->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBF,true,RR_GHOST_BUFFER);
			accumulationMap->bindTexture();
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGB32F_ARB,w,h,0,GL_RGB,GL_FLOAT,NULL); // GL_RGB32F_ARB prevents banding, simple accumulationMap->reset() would use GL_RGB16F_ARB
		}

		{
			PreserveFBO p0;

			// brightnessAdjustment gives us HDR even when we render to 8bit for majority of time
			//  1 = no adjustment, intensity clamped at 1 as usual, DOF bokeh hardly visible
			//  0.1 = intensity clamped at 10, bright spots create bokeh, but banding is visible (because frames go through ~4.5 bits of 8bit buffer)
			//  0.1f*(rand()+RAND_MAX)/RAND_MAX = intensity clamped at ~10, bright spots create bokeh, banding disappears after averaging several frames
			//  1/(1+accumulatedBrightness) = intensity first clamped at 1 but increases later, bright spots create bokeh. no banding at first, but it shows up after long time
			const float brightnessAdjustment = (numAccumulatedFrames<7)
				? 1/(1.f+numAccumulatedFrames)
				: 0.1f*(rand()+RAND_MAX)/RAND_MAX;
	
			// render to colorMap (c^2.2 * brightnessAdjustment^2.2)
			colorMap->reset(false,false,_sp.srgbCorrect);
			FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthMap,p0.state);
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colorMap,p0.state);
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			PluginParamsShared sp = _sp;
			sp.brightness *= brightnessAdjustment;
			sp.viewport[0] = 0;
			sp.viewport[1] = 1;
			glViewport(0,0,w,h);
			_renderer.render(_pp.next,sp);

			// add colorMap (c^2.2 * brightnessAdjustment^2.2) to accumulation buffer
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,accumulationMap,p0.state);
			PreserveDepthTest p2;
			PreserveDepthMask p3;
			PreserveFlag p4(GL_SCISSOR_TEST,false);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(0);
			PreserveFlag p5(GL_BLEND,numAccumulatedFrames?true:false);
			glBlendFunc(GL_ONE, GL_ONE);
			_renderer.getTextureRenderer()->render2D(colorMap,NULL,0,0,1,1,-1);
			glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
			numAccumulatedFrames++;
			accumulatedBrightness += _sp.srgbCorrect ? pow(brightnessAdjustment,2.2f) : brightnessAdjustment;
		}

		// copy accumulation buffer back to render target (divided by accumulated brightnessAdjustment^2.2)
		PreserveFlag p6(GL_FRAMEBUFFER_SRGB,_sp.srgbCorrect);
		ToneParameters tp;
		tp.color = rr::RRVec4(1.f/accumulatedBrightness);
		_renderer.getTextureRenderer()->render2D(accumulationMap,&tp,0,0,1,1);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsAccumulation

PluginRuntime* PluginParamsAccumulation::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeAccumulation(params);
}

#endif //!RR_GL_ES2

}; // namespace
