// --------------------------------------------------------------------------
// DemoEngine
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <GL/glew.h>
#include "DemoEngine/TextureRenderer.h"
#include "DemoEngine/Program.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer

TextureRenderer::TextureRenderer(const char* pathToShaders)
{
	char buf1[400];
	char buf2[400];
	_snprintf(buf1,999,"%ssky.vp",pathToShaders);
	_snprintf(buf2,999,"%ssky.fp",pathToShaders);
	skyProgram = de::Program::create(NULL,buf1,buf2);
	if(!skyProgram) printf("Helper shaders failed: %s/sky.*\n",pathToShaders);
}

TextureRenderer::~TextureRenderer()
{
	delete skyProgram;
}

void TextureRenderer::renderEnvironment(Texture* texture)
{
	if(!texture || !skyProgram)
	{
		assert(0);
		return;
	}
	// backup render states
	GLboolean culling = glIsEnabled(GL_CULL_FACE);
	GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
	GLboolean depthMask;
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	// setup render states
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	// render cube
	skyProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	texture->bindTexture();
	skyProgram->sendUniform("cube",0);
//	skyProgram->sendUniform("worldEyePos",worldEyePos[0],worldEyePos[1],worldEyePos[2]);
	glBegin(GL_POLYGON);
		glVertex3f(-1,-1,1);
		glVertex3f(1,-1,1);
		glVertex3f(1,1,1);
		glVertex3f(-1,1,1);
	glEnd();
	// restore render states
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
	if(culling) glEnable(GL_CULL_FACE);
};

}; // namespace
