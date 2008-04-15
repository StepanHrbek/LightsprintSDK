// --------------------------------------------------------------------------
// Water with reflection and waves.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------
/*
#include <GL/glew.h>
#include <cstdio>
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/Timer.h"

namespace rr_gl
{

Water::Water(const char* pathToShaders, bool afresnel, bool boostSun)
{
	mirrorMap = Texture::create(NULL,1,1,false,Texture::TF_RGBF,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	mirrorDepth = Texture::createShadowmap(1,1);
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
	delete mirrorMap;
}

void Water::updateReflectionInit(unsigned reflWidth, unsigned reflHeight, Camera* aeye, float aaltitude)
{
	if(!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	// adjust size
	if(reflWidth!=mirrorMap->getWidth() || reflHeight!=mirrorMap->getHeight())
	{
		// TF_RGBF improves HDR sun reflection (TF_RGBA = LDR reflection is weak),
		//  but float/short render target is not supported by GF6100-6200 and Rad9500-?
		mirrorMap->reset(reflWidth,reflHeight,Texture::TF_RGBA,NULL,false);
		mirrorDepth->reset(reflWidth,reflHeight,Texture::TF_NONE,NULL,false);
	}
	mirrorDepth->renderingToBegin();
	mirrorMap->renderingToBegin();
	glGetIntegerv(GL_VIEWPORT,viewport);
	glViewport(0,0,mirrorMap->getWidth(),mirrorMap->getHeight());
	eye = aeye;
	altitude = aaltitude;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	GLdouble plane[4] = {0,1,0,-altitude};
	glClipPlane(GL_CLIP_PLANE0,plane);
	glEnable(GL_CLIP_PLANE0);

	if(eye)
	{
		eye->mirror(altitude);
		eye->update();
		eye->setupForRender();
	}
}

void Water::updateReflectionDone()
{
	glDisable(GL_CLIP_PLANE0);
	if(!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorDepth->renderingToEnd();
	mirrorMap->renderingToEnd();
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	if(eye)
	{
		eye->mirror(altitude);
		eye->update();
	}
}

void Water::render(float size)
{
	if(!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	mirrorMap->bindTexture();
	mirrorProgram->sendUniform("mirrorMap",0);
	mirrorProgram->sendUniform("time",(float)(GETTIME/(float)PER_SEC));
	if(fresnel) mirrorProgram->sendUniform("worldEyePos",eye->pos[0],eye->pos[1],eye->pos[2]);
	//GLboolean blend = glIsEnabled(GL_BLEND);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(size>0)
	{
		glBegin(GL_QUADS);
		glVertex3f(-size,altitude,-size);
		glVertex3f(-size,altitude,size);
		glVertex3f(size,altitude,size);
		glVertex3f(size,altitude,-size);
		glEnd();
	}
	//if(!blend)
	//	glDisable(GL_BLEND);
}

}; // namespace
*/