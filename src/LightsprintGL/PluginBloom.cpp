// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Bloom postprocess.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginBloom.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeBloom

class PluginRuntimeBloom : public PluginRuntime
{
	Texture* bigMap;
	Texture* smallMap1;
	Texture* smallMap2;
	Program* scaleDownProgram;
	Program* blurProgram;
	Program* blendProgram;

public:

	PluginRuntimeBloom(const PluginCreateRuntimeParams& params)
	{
		bigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		smallMap1 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		smallMap2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		rr::RRString filename1(0,L"%lsbloom.vs",params.pathToShaders.w_str());
		rr::RRString filename2(0,L"%lsbloom.fs",params.pathToShaders.w_str());
		scaleDownProgram = Program::create("#define PASS 1\n",filename1,filename2);
		blurProgram = Program::create("#define PASS 2\n",filename1,filename2);
		blendProgram = Program::create("#define PASS 3\n",filename1,filename2);
	}

	virtual ~PluginRuntimeBloom()
	{
		delete blendProgram;
		delete blurProgram;
		delete scaleDownProgram;
		delete smallMap2;
		delete smallMap1;
		delete bigMap;
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsBloom& pp = *dynamic_cast<const PluginParamsBloom*>(&_pp);
		
		if (!bigMap || !smallMap1 || !smallMap2 || !scaleDownProgram || !blurProgram || !blendProgram) return;

		FBO oldFBOState = FBO::getState();

		// adjust map sizes to match render target size
		unsigned w = _sp.viewport[2];
		unsigned h = _sp.viewport[3];
		if (w!=bigMap->getBuffer()->getWidth() || h!=bigMap->getBuffer()->getHeight())
		{
			bigMap->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
#ifdef OPTIMIZE_BLOOM
			if (!oldFBOState.color_id)
#endif
				bigMap->reset(false,false,false);
			smallMap1->getBuffer()->reset(rr::BT_2D_TEXTURE,w/4,h/4,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
			smallMap1->reset(false,false,false);
			smallMap2->getBuffer()->reset(rr::BT_2D_TEXTURE,w/4,h/4,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
			smallMap2->reset(false,false,false);
		}
	
		// disable depth
		PreserveDepthTest p1;
		PreserveDepthMask p2;
		PreserveFlag p3(GL_SCISSOR_TEST,false);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);

		// acquire bigMap
#ifdef OPTIMIZE_BLOOM
		unsigned oldBigMapId;
		if (oldFBOState.color_id)
		{
			// use color texture currently assigned to FBO
			oldBigMapId = bigMap->id;
			bigMap->id = oldFBOState.color_id;
		}
		else
#endif
		{
			// copy backbuffer to bigMap
			bigMap->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,_sp.viewport[0],_sp.viewport[1],w,h,0);
		}

		// downscale bigMap to smallMap1
		FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,nullptr,oldFBOState);
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallMap1,oldFBOState);
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("map",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/bigMap->getBuffer()->getWidth(),1.0f/bigMap->getBuffer()->getHeight());
		scaleDownProgram->sendUniform("threshold",3.0f*pp.threshold);
		glViewport(0,0,smallMap1->getBuffer()->getWidth(),smallMap1->getBuffer()->getHeight());
		glActiveTexture(GL_TEXTURE0);
		bigMap->bindTexture();
		glDisable(GL_CULL_FACE);
		TextureRenderer::renderQuad();

		// horizontal blur smallMap1 to smallMap2
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallMap2,oldFBOState);
		blurProgram->useIt();
		blurProgram->sendUniform("map",0);
		blurProgram->sendUniform("pixelDistance",0.0f,1.0f/smallMap1->getBuffer()->getHeight());
		smallMap1->bindTexture();
		TextureRenderer::renderQuad();
	
		// vertical blur smallMap2 to smallMap1
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallMap1,oldFBOState);
		blurProgram->sendUniform("pixelDistance",1.0f/smallMap1->getBuffer()->getWidth(),0.0f);
		smallMap2->bindTexture();
		TextureRenderer::renderQuad();

		// blend smallMap1 to render target
		oldFBOState.restore();
		blendProgram->useIt();
		blendProgram->sendUniform("map",0);
		glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		smallMap1->bindTexture();
		TextureRenderer::renderQuad();
		glDisable(GL_BLEND);

#ifdef OPTIMIZE_BLOOM
		if (oldFBOState.color_id)
		{
			bigMap->id = oldBigMapId;
		}
#endif
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsBloom

PluginRuntime* PluginParamsBloom::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeBloom(params);
}

}; // namespace
