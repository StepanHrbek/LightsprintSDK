// --------------------------------------------------------------------------
// Bloom postprocess.
// Copyright (C) 2010-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/Bloom.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"

namespace rr_gl
{

//#define OPTIMIZE_BLOOM // optional optimization

Bloom::Bloom(const rr::RRString& pathToShaders)
{
	bigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallMap1 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallMap2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	rr::RRString filename1(0,L"%lsbloom.vs",pathToShaders.w_str());
	rr::RRString filename2(0,L"%lsbloom.fs",pathToShaders.w_str());
	scaleDownProgram = Program::create("#define PASS 1\n",filename1,filename2);
	blurProgram = Program::create("#define PASS 2\n",filename1,filename2);
	blendProgram = Program::create("#define PASS 3\n",filename1,filename2);
}

Bloom::~Bloom()
{
	delete blendProgram;
	delete blurProgram;
	delete scaleDownProgram;
	delete smallMap2;
	delete smallMap1;
	delete bigMap;
}

void Bloom::applyBloom(unsigned _w, unsigned _h, float _threshold)
{
	if (!bigMap || !smallMap1 || !smallMap2 || !blurProgram || !scaleDownProgram) return;

	FBO oldFBOState = FBO::getState();

	// adjust map sizes to match render target size
	if (_w!=bigMap->getBuffer()->getWidth() || _h!=bigMap->getBuffer()->getHeight())
	{
		bigMap->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
#ifdef OPTIMIZE_BLOOM
		if (!oldFBOState.color_id)
#endif
			bigMap->reset(false,false,false);
		smallMap1->getBuffer()->reset(rr::BT_2D_TEXTURE,_w/4,_h/4,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		smallMap1->reset(false,false,false);
		smallMap2->getBuffer()->reset(rr::BT_2D_TEXTURE,_w/4,_h/4,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		smallMap2->reset(false,false,false);
	}
	
	// disable depth
	PreserveDepthTest p1;
	PreserveDepthMask p2;
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
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,_w,_h,0);
	}

	// downscale bigMap to smallMap1
	FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,NULL);
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallMap1);
	scaleDownProgram->useIt();
	scaleDownProgram->sendUniform("map",0);
	scaleDownProgram->sendUniform("pixelDistance",1.0f/bigMap->getBuffer()->getWidth(),1.0f/bigMap->getBuffer()->getHeight());
	scaleDownProgram->sendUniform("threshold",3.0f*_threshold);
	glViewport(0,0,smallMap1->getBuffer()->getWidth(),smallMap1->getBuffer()->getHeight());
	glActiveTexture(GL_TEXTURE0);
	bigMap->bindTexture();
	glDisable(GL_CULL_FACE);
	TextureRenderer::renderQuad();

	// horizontal blur smallMap1 to smallMap2
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallMap2);
	blurProgram->useIt();
	blurProgram->sendUniform("map",0);
	blurProgram->sendUniform("pixelDistance",0.0f,1.0f/smallMap1->getBuffer()->getHeight());
	smallMap1->bindTexture();
	TextureRenderer::renderQuad();
	
	// vertical blur smallMap2 to smallMap1
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallMap1);
	blurProgram->sendUniform("pixelDistance",1.0f/smallMap1->getBuffer()->getWidth(),0.0f);
	smallMap2->bindTexture();
	TextureRenderer::renderQuad();

	// blend smallMap1 to render target
	oldFBOState.restore();
	blendProgram->useIt();
	blendProgram->sendUniform("map",0);
	glViewport(0,0,bigMap->getBuffer()->getWidth(),bigMap->getBuffer()->getHeight());
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

}; // namespace
