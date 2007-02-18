// --------------------------------------------------------------------------
// DemoEngine
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "DemoEngine/TextureRenderer.h"
#include "DemoEngine/Program.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer

TextureRenderer::TextureRenderer()
{
	skyProgram = new Program("","shaders\\sky.vp", "shaders\\sky.fp");
}

TextureRenderer::~TextureRenderer()
{
	delete skyProgram;
}

void TextureRenderer::renderEnvironment(Texture* texture)
{
	if(!texture)
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
