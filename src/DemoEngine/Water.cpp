// --------------------------------------------------------------------------
// DemoEngine
// Water with reflection and waves.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include <cstdio>
#include "Lightsprint/DemoEngine/Water.h"
#include <windows.h>

namespace de
{

Water::Water(const char* pathToShaders)
{
	mirrorMap = de::Texture::create(NULL,1,1,false,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	mirrorDepth = de::Texture::createShadowmap(1,1);
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%swater.vs",pathToShaders);
	_snprintf(buf2,399,"%swater.fs",pathToShaders);
	mirrorProgram = de::Program::create(NULL,buf1,buf2);
	eye = NULL;
	altitude = 0;
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
		mirrorMap->reset(reflWidth,reflHeight,Texture::TF_RGBA,NULL,false);
		mirrorDepth->reset(reflWidth,reflHeight,Texture::TF_NONE,NULL,false);
	}
	mirrorDepth->renderingToBegin();
	mirrorMap->renderingToBegin();
	glGetIntegerv(GL_VIEWPORT,viewport);
	glViewport(0,0,mirrorMap->getWidth(),mirrorMap->getHeight());
	//!!! clipping
	eye = aeye;
	altitude = aaltitude;
	if(eye)
	{
		eye->mirror(altitude);
		eye->update(0);
		eye->setupForRender();
	}
}

void Water::updateReflectionDone()
{
	if(!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorDepth->renderingToEnd();
	mirrorMap->renderingToEnd();
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	if(eye)
	{
		eye->mirror(altitude);
		eye->update(0);
	}
}

void Water::render(float size)
{
	if(!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	mirrorMap->bindTexture();
	mirrorProgram->sendUniform("mirrorMap",0);
	mirrorProgram->sendUniform("time",(timeGetTime()%10000000)*0.001f);
	if(size>0)
	{
		glBegin(GL_QUADS);
		glVertex3f(-size,altitude,-size);
		glVertex3f(-size,altitude,size);
		glVertex3f(size,altitude,size);
		glVertex3f(size,altitude,-size);
		glEnd();
	}
}

}; // namespace
