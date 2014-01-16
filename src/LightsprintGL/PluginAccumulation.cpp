// --------------------------------------------------------------------------
// Accumulates frames, outputs average.
// Copyright (C) 2010-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginAccumulation.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

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

	PluginRuntimeAccumulation(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
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
			FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,depthMap);
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,colorMap);
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			PluginParamsShared sp = _sp;
			sp.brightness *= brightnessAdjustment;
			sp.viewport[0] = 0;
			sp.viewport[1] = 1;
			glViewport(0,0,w,h);
			_renderer.render(_pp.next,sp);

			// add colorMap (c^2.2 * brightnessAdjustment^2.2) to accumulation buffer
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,accumulationMap);
			PreserveDepthTest p2;
			PreserveDepthMask p3;
			PreserveFlag p4(GL_SCISSOR_TEST,false);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(0);
			PreserveFlag p5(GL_BLEND,numAccumulatedFrames?true:false);
			glBlendFunc(GL_ONE, GL_ONE);
			_renderer.getTextureRenderer()->render2D(colorMap,NULL,1,0,0,1,1,-1);
			glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
			numAccumulatedFrames++;
			accumulatedBrightness += _sp.srgbCorrect ? pow(brightnessAdjustment,2.2f) : brightnessAdjustment;
		}

		// copy accumulation buffer back to render target (divided by accumulated brightnessAdjustment^2.2)
		PreserveFlag p6(GL_FRAMEBUFFER_SRGB,_sp.srgbCorrect);
		rr::RRVec4 color(1.f/accumulatedBrightness);
		_renderer.getTextureRenderer()->render2D(accumulationMap,&color,1,0,0,1,1);
	}
	/*
	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
// problem s brightnessAdjustment!=1 (1 nedela nic):
//  kdyz renderuju ztmavenou scenu do srgb framebufferu, neni snadne ji zas zesvetlit, potreboval bych z toho srgb bufferu zas nasamplovat co jsem do nej zapsal
//  kdyz ji nactu pomoci glCopyTexImage2D, inverzni srgb transformaci musim udelat sam (abych se spravne zbavil brightnessAdjustment) a to se mi nedari, vysledek se lisi
//  (cokoliv z) +-bloom, +-vign, +-stereo, +-ssgi: nema vliv, chyba trva
//  (cokoliv z) -srgb, +pano: spravny vysledek
//				pano vola ppScene(jedinej render do srgb) nad stenama cubemapy
//				chyba vznika jen kdyz aspon chvili renderuju srgb do backbufferu
// coz renderovat do srgb textury? z ni pak muzu samplovat puvodni hodnoty
		PluginParamsShared sp = _sp;
		const float brightnessAdjustment = 1;
		sp.brightness *= brightnessAdjustment;

		_renderer.render(_pp.next,sp);

		bool srgbCorrect = _sp.srgbCorrect; // can be changed to true, correct accumulation is slightly more expensive, but works even without sRGB textures (i.e. in GL ES 2.0)

		const PluginParamsAccumulation& pp = *dynamic_cast<const PluginParamsAccumulation*>(&_pp);
		
		if (!colorMap || !accumulationMap) return;

		if (pp.restart)
		{
			numAccumulatedFrames = 0;
		}

		// adjust map sizes to match render target size
		unsigned w = _sp.viewport[2];
		unsigned h = _sp.viewport[3];
		if (w!=colorMap->getBuffer()->getWidth() || h!=colorMap->getBuffer()->getHeight())
		{
			numAccumulatedFrames = 0;
			colorMap->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGB,true,RR_GHOST_BUFFER);
			//colorMap->reset(false,false,false);
			accumulationMap->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBF,true,RR_GHOST_BUFFER);
			#define fastFirstFrame (!srgbCorrect && !numAccumulatedFrames && brightnessAdjustment==1)
			if (!fastFirstFrame) // !fastFirstFrame: texture inited here. fastFirstFrame: inited later by glCopyTexImage2D
			{
				//accumulationMap->reset(false,false,false); <- not good enough, sets GL_RGB16F_ARB and banding would be visible
				accumulationMap->bindTexture();
				glTexImage2D(GL_TEXTURE_2D,0,GL_RGB32F_ARB,w,h,0,GL_RGB,GL_FLOAT,NULL); // GL_RGB32F_ARB prevents banding
			}
		}
	
		if (fastFirstFrame)
		{
			accumulationMap->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB32F_ARB,_sp.viewport[0],_sp.viewport[1],w,h,0); // banding is clearly visible with GL_RGB16F_ARB, 32F is ok
			numAccumulatedFrames++;
		}
		else
		{
			// disable depth
//			PreserveDepthTest p1;
			PreserveDepthMask p2;
			PreserveFlag p3(GL_SCISSOR_TEST,false);
//			glDisable(GL_DEPTH_TEST);
			glDepthMask(0);

			// acquire current frame
			colorMap->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,_sp.viewport[0],_sp.viewport[1],w,h,0);

			// add current frame to accumulation buffer
			float gamma = srgbCorrect?2.222f:1.f;
			FBO oldFBOState = FBO::getState();
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,accumulationMap);
			{
				PreserveFlag p3(GL_BLEND,numAccumulatedFrames?true:false);
				glBlendFunc(GL_ONE, GL_ONE);
				rr::RRVec4 color(pow(1/brightnessAdjustment,sp.gamma));
				_renderer.getTextureRenderer()->render2D(colorMap,&color,gamma,0,0,1,1,-1);// "#define BOOST_WHITE 2.0\n"); 
			}
			oldFBOState.restore();
			numAccumulatedFrames++;

			// copy accumulation buffer back to render target
			rr::RRVec4 color(1.f/numAccumulatedFrames);
			_renderer.getTextureRenderer()->render2D(accumulationMap,&color,1/gamma,0,0,1,1);
		}
	}*/
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsAccumulation

PluginRuntime* PluginParamsAccumulation::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeAccumulation(pathToShaders, pathToMaps);
}

}; // namespace
