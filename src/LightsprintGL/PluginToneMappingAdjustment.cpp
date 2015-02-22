// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Tone mapping adjustment
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginToneMappingAdjustment.h"
//#include "Lightsprint/GL/PreserveState.h"
//#include <cstdlib>
#include "Lightsprint/GL/FBO.h"

#define PBO // not present in core OpenGL ES 2.0
#define SMALL_W 32
#define SMALL_H 32
#define SMALL_E 3 // 3 for RGB, 4 for RGBA

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeToneMappingAdjustment

class PluginRuntimeToneMappingAdjustment : public PluginRuntime
{
	Texture* bigTexture;
	Texture* smallTexture;
#ifdef PBO
	GLuint pbo[2];
	rr::RRTime pboTime;
	unsigned pboIndex;
#endif

public:

	PluginRuntimeToneMappingAdjustment(const PluginCreateRuntimeParams& params)
	{
		bigTexture = nullptr;
		smallTexture = nullptr;
#ifdef PBO
		glGenBuffers(2,pbo);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pbo[0]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, SMALL_W*SMALL_H*SMALL_E, 0, GL_STREAM_READ_ARB);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pbo[1]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, SMALL_W*SMALL_H*SMALL_E, 0, GL_STREAM_READ_ARB);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		pboIndex = 0;
		pboTime.addSeconds(-10);
#endif
	}

	virtual ~PluginRuntimeToneMappingAdjustment()
	{
#ifdef PBO
		glDeleteBuffers(2,pbo);
#endif
		if (smallTexture)
		{
			delete smallTexture->getBuffer();
			delete smallTexture;
		}
		if (bigTexture)
		{
			delete bigTexture->getBuffer();
			delete bigTexture;
		}
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsToneMappingAdjustment& pp = *dynamic_cast<const PluginParamsToneMappingAdjustment*>(&_pp);

		if (!_renderer.getTextureRenderer()) return;

		if (!bigTexture) bigTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,nullptr),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		bigTexture->bindTexture();
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3],0);

#ifndef PBO
		unsigned char buf[SMALL_W*SMALL_H*SMALL_E];
#endif
		unsigned histo[256];
		unsigned avg = 0;
		if (!smallTexture)
		{
			smallTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,SMALL_W,SMALL_H,1,rr::BF_RGB,true,nullptr),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		}
		FBO oldFBOState = FBO::getState();
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallTexture,oldFBOState);
		glViewport(0,0,SMALL_W,SMALL_H);
		_renderer.getTextureRenderer()->render2D(bigTexture,nullptr,0,0,1,1);
#ifdef PBO
		glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo[pboIndex]);
		glReadPixels(0,0,SMALL_W,SMALL_H,GL_RGB,GL_UNSIGNED_BYTE,0);
#else
		glReadPixels(0,0,SMALL_W,SMALL_H,GL_RGB,GL_UNSIGNED_BYTE,buf);
#endif

		oldFBOState.restore();
		glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);
#ifdef PBO
		glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo[1-pboIndex]);
		GLubyte* buf = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER,GL_READ_ONLY);
		if(buf)
		{
			if (pboTime.secondsSinceLastQuery()<2) // don't adjust based on PBO we started reading more than 2sec ago (prevents adjust based on initial empty buffer)
			{
#endif
				for (unsigned i=0;i<256;i++)
					histo[i] = 0;
				for (unsigned i=0;i<SMALL_W*SMALL_H*SMALL_E;i+=SMALL_E)
					histo[RR_MAX3(buf[i],buf[i+1],buf[i+2])]++;
				for (unsigned i=0;i<256;i++)
					avg += histo[i]*i;
				if (avg==0)
				{
					// completely black scene
					//  extremely low brightness? -> reset it to 1
					//  disabled lighting? -> avoid increasing brightness ad infinitum
					pp.brightness = pp.brightness / (pp.brightness.RRVec3::avg()?pp.brightness.RRVec3::avg():1);
				}
				else
				{
					avg = avg/(SMALL_W*SMALL_H)+1;
					if (histo[255]>=SMALL_W*SMALL_H*7/10) avg = 1000; // at least 70% of screen overshot, adjust faster
					pp.brightness *= pow(pp.targetIntensity*255/avg,RR_CLAMPED(pp.secondsSinceLastAdjustment*0.15f,0.0002f,0.2f));
				}
#ifdef PBO
			}
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		}
		pboIndex = 1-pboIndex;
		glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
#endif
		//rr::RRReporter::report(rr::INF1,"%d\n",avg);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsToneMappingAdjustment

PluginRuntime* PluginParamsToneMappingAdjustment::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeToneMappingAdjustment(params);
}

}; // namespace
