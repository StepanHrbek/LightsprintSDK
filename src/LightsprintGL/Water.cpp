// --------------------------------------------------------------------------
// Water with reflection and waves.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include <cstdio>
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/Timer.h"
//#include "Lightsprint/GL/TextureRenderer.h"

namespace rr_gl
{

Water::Water(const char* pathToShaders, bool afresnel, bool boostSun)
{
	mirrorMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,NULL),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	mirrorDepth = Texture::createShadowmap(16,16);
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	char buf3[400]; buf3[399] = 0;
	_snprintf(buf1,399,"%swater.vs",pathToShaders);
	_snprintf(buf2,399,"%swater.fs",pathToShaders);
	_snprintf(buf3,399,"%s%s",afresnel?"#define FRESNEL\n":"",boostSun?"#define BOOST_SUN\n":"");
	mirrorProgram = Program::create(buf3,buf1,buf2);
	eye = NULL;
	altitude = 0;
	fresnel = afresnel;
}

Water::~Water()
{
	delete mirrorProgram;
	delete mirrorDepth;
	delete mirrorMap->getBuffer();
	delete mirrorMap;
}

void Water::updateReflectionInit(unsigned _reflWidth, unsigned _reflHeight, Camera* _eye, float _altitude)
{
	if (!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	// adjust size
	if (_reflWidth!=mirrorMap->getBuffer()->getWidth() || _reflHeight!=mirrorMap->getBuffer()->getHeight())
	{
		// RGBF improves HDR sun reflection (RGBA = LDR reflection is weak),
		//  but float/short render target is not supported by GF6100-6200 and Rad9500-?
		mirrorMap->getBuffer()->reset(rr::BT_2D_TEXTURE,_reflWidth,_reflHeight,1,rr::BF_RGBA,true,NULL);
		mirrorMap->reset(false,false);
		mirrorDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,_reflWidth,_reflHeight,1,rr::BF_DEPTH,true,NULL);
		mirrorDepth->reset(false,false);
	}
	mirrorDepth->renderingToBegin();
	mirrorMap->renderingToBegin();
	glGetIntegerv(GL_VIEWPORT,viewport);
	glViewport(0,0,mirrorMap->getBuffer()->getWidth(),mirrorMap->getBuffer()->getHeight());
	eye = _eye;
	altitude = _altitude;

	if (eye)
	{
		eye->mirror(altitude);
		eye->update();
		eye->setupForRender();
	}
}

void Water::updateReflectionDone()
{
	if (!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorDepth->renderingToEnd();
	mirrorMap->renderingToEnd();
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	if (eye)
	{
		eye->mirror(altitude);
		eye->update();
	}
}

void Water::render(float size, rr::RRVec3 center)
{
	if (!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	mirrorMap->bindTexture();
	mirrorProgram->sendUniform("mirrorMap",0);
	mirrorProgram->sendUniform("time",(float)(GETTIME/(float)PER_SEC));
	if (fresnel) mirrorProgram->sendUniform("worldEyePos",eye->pos[0],eye->pos[1],eye->pos[2]);
	//GLboolean blend = glIsEnabled(GL_BLEND);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (size>0)
	{
		glBegin(GL_QUADS);
		glVertex3f(center[0]-size,altitude,center[2]-size);
		glVertex3f(center[0]-size,altitude,center[2]+size);
		glVertex3f(center[0]+size,altitude,center[2]+size);
		glVertex3f(center[0]+size,altitude,center[2]-size);
		glEnd();
	}
	//if (!blend)
	//	glDisable(GL_BLEND);

	//static TextureRenderer* textureRenderer = NULL;
	//if (!textureRenderer) textureRenderer = new TextureRenderer("../../data/shaders/");
	//textureRenderer->render2D(mirrorMap,NULL,0,0,0.3,0.3);
}

}; // namespace
