// --------------------------------------------------------------------------
// Water with planar reflection, fresnel, animated waves.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include <cstdio>
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/Timer.h"
//#include "Lightsprint/GL/TextureRenderer.h"
#include "tmpstr.h"

namespace rr_gl
{

Water::Water(const char* pathToShaders, bool _fresnel, bool _dirlight)
{
	normalMap = new Texture(rr::RRBuffer::load(tmpstr("%s../maps/water_n.png",pathToShaders)),true,false);
	mirrorMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,NULL),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	mirrorDepth = Texture::createShadowmap(16,16);
	mirrorProgram = Program::create(
		tmpstr("%s%s",_fresnel?"#define FRESNEL\n":"",_dirlight?"#define DIRLIGHT\n":""),
		tmpstr("%swater.vs",pathToShaders),
		tmpstr("%swater.fs",pathToShaders));
	eye = NULL;
	altitude = 0;
	fresnel = _fresnel;
	dirlight = _dirlight;
}

Water::~Water()
{
	delete mirrorProgram;
	delete mirrorDepth;
	delete mirrorMap->getBuffer();
	delete mirrorMap;
	delete normalMap->getBuffer();
	delete normalMap;
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
	oldFBOState = FBO::getState();
	FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,mirrorDepth);
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,mirrorMap);
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
	oldFBOState.restore();
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	if (eye)
	{
		eye->mirror(altitude);
		eye->update();
		eye->setupForRender();
	}
}

void Water::render(float size, rr::RRVec3 center, rr::RRVec4 waterColor, rr::RRVec3 lightDirection, rr::RRVec3 lightColor)
{
	if (!mirrorMap || !mirrorDepth || !mirrorProgram) return;
	mirrorProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	mirrorMap->bindTexture();
	mirrorProgram->sendUniform("mirrorMap",0);
	glActiveTexture(GL_TEXTURE1);
	normalMap->bindTexture();
	mirrorProgram->sendUniform("normalMap",1);
	mirrorProgram->sendUniform("time",(float)(fmod(GETSEC,10000)));
	mirrorProgram->sendUniform("waterColor",waterColor[0],waterColor[1],waterColor[2],waterColor[3]);
	if (dirlight || fresnel)
	{
		mirrorProgram->sendUniform("worldEyePos",eye->pos[0],eye->pos[1],eye->pos[2]);
	}
	if (dirlight)
	{
		if (lightDirection!=rr::RRVec3(0)) lightDirection.normalize();
		mirrorProgram->sendUniform("worldLightDir",lightDirection[0],lightDirection[1],lightDirection[2]);
		mirrorProgram->sendUniform("lightColor",lightColor[0],lightColor[1],lightColor[2]);
	}
	//GLboolean blend = glIsEnabled(GL_BLEND);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (size>0)
	{
		glBegin(GL_TRIANGLES);
		// rendering one huge triangle is not good, rounding errors in shader might be visible,
		// smaller triangles improve precision in near water
		for (int i=10;i>=0;i--)
		{
			float localsize = size * expf((float)-i);
			float localalt = altitude + i*(size*1e-7f);
			glVertex3f(center[0]-0.5f*localsize,localalt,center[2]-0.86602540378444f*localsize);
			glVertex3f(center[0]-0.5f*localsize,localalt,center[2]+0.86602540378444f*localsize);
			glVertex3f(center[0]+localsize,localalt,center[2]);
		}
		glEnd();
	}
	//if (!blend)
	//	glDisable(GL_BLEND);

	//static TextureRenderer* textureRenderer = NULL;
	//if (!textureRenderer) textureRenderer = new TextureRenderer("../../data/shaders/");
	//textureRenderer->render2D(mirrorMap,NULL,0,0,0.3f,0.3f);
}

}; // namespace
