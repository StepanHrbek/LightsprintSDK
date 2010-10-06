// --------------------------------------------------------------------------
// Bloom postprocess.
// Copyright (C) 2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/Bloom.h"
#include "PreserveState.h"
#include "tmpstr.h"

namespace rr_gl
{

//#define OPTIMIZE_BLOOM // optional optimization
#define NO_SYSTEM_MEMORY (unsigned char*)1

Bloom::Bloom(const char* pathToShaders)
{
	bigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,NO_SYSTEM_MEMORY),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallMap1 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,NO_SYSTEM_MEMORY),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallMap2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,NO_SYSTEM_MEMORY),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	scaleDownProgram = Program::create("#define PASS 1\n",tmpstr("%sbloom.vs",pathToShaders),tmpstr("%sbloom.fs",pathToShaders));
	blurProgram = Program::create("#define PASS 2\n",tmpstr("%sbloom.vs",pathToShaders),tmpstr("%sbloom.fs",pathToShaders));
	blendProgram = Program::create("#define PASS 3\n",tmpstr("%sbloom.vs",pathToShaders),tmpstr("%sbloom.fs",pathToShaders));
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

void Bloom::applyBloom(unsigned _w, unsigned _h)
{
	if (!bigMap || !smallMap1 || !smallMap2 || !blurProgram || !scaleDownProgram) return;

	FBO oldFBOState = FBO::getState();

	// adjust map sizes to match render target size
	if (_w!=bigMap->getBuffer()->getWidth() || _h!=bigMap->getBuffer()->getHeight())
	{
		bigMap->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,NO_SYSTEM_MEMORY);
#ifdef OPTIMIZE_BLOOM
		if (!oldFBOState.color_id)
#endif
			bigMap->reset(false,false);
		smallMap1->getBuffer()->reset(rr::BT_2D_TEXTURE,_w/4,_h/4,1,rr::BF_RGBA,true,NO_SYSTEM_MEMORY);
		smallMap1->reset(false,false);
		smallMap2->getBuffer()->reset(rr::BT_2D_TEXTURE,_w/4,_h/4,1,rr::BF_RGBA,true,NO_SYSTEM_MEMORY);
		smallMap2->reset(false,false);
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
	glViewport(0,0,smallMap1->getBuffer()->getWidth(),smallMap1->getBuffer()->getHeight());
	glActiveTexture(GL_TEXTURE0);
	bigMap->bindTexture();
	glDisable(GL_CULL_FACE);
	glBegin(GL_POLYGON);
		glVertex2f(-1,-1);
		glVertex2f(-1,1);
		glVertex2f(1,1);
		glVertex2f(1,-1);
	glEnd();

	// horizontal blur smallMap1 to smallMap2
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallMap2);
	blurProgram->useIt();
	blurProgram->sendUniform("map",0);
	blurProgram->sendUniform("pixelDistance",0.0f,1.0f/smallMap1->getBuffer()->getHeight());
	smallMap1->bindTexture();
	glBegin(GL_POLYGON);
		glVertex2f(-1,-1);
		glVertex2f(-1,1);
		glVertex2f(1,1);
		glVertex2f(1,-1);
	glEnd();
	
	// vertical blur smallMap2 to smallMap1
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallMap1);
	blurProgram->sendUniform("pixelDistance",1.0f/smallMap1->getBuffer()->getWidth(),0.0f);
	smallMap2->bindTexture();
	glBegin(GL_POLYGON);
		glVertex2f(-1,-1);
		glVertex2f(-1,1);
		glVertex2f(1,1);
		glVertex2f(1,-1);
	glEnd();
	
	// blend smallMap1 to render target
	oldFBOState.restore();
	blendProgram->useIt();
	blendProgram->sendUniform("map",0);
	glViewport(0,0,bigMap->getBuffer()->getWidth(),bigMap->getBuffer()->getHeight());
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	smallMap1->bindTexture();
	glBegin(GL_POLYGON);
		glVertex2f(-1,-1);
		glVertex2f(-1,1);
		glVertex2f(1,1);
		glVertex2f(1,-1);
	glEnd();
	glDisable(GL_BLEND);

#ifdef OPTIMIZE_BLOOM
	if (oldFBOState.color_id)
	{
		bigMap->id = oldBigMapId;
	}
#endif
}

}; // namespace
