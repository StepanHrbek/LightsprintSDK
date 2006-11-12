// --------------------------------------------------------------------------
// DemoEngine
// Implementation of RRIlluminationEnvironmentMapInOpenGL
// Copyright (C) Stepan Hrbek, Lightsprint, 2006
// --------------------------------------------------------------------------

#include "DemoEngine/RRIlluminationEnvironmentMapInOpenGL.h"
#include "DemoEngine/Program.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInOpenGL

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(unsigned asize)
{
	// creates cube map
	texture = Texture::create(NULL,asize,asize,true,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
}

void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, RRColorRGBA8* irradiance)
{
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(unsigned side=0; side<6; side++)
	{
		//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, GL_RGBA8, size, size, GL_RGBA, GL_UNSIGNED_BYTE, &irradiance[size*size*side].color);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,&irradiance[size*size*side].color);
	}
}

void RRIlluminationEnvironmentMapInOpenGL::bindTexture()
{
	texture->bindTexture();
}

RRIlluminationEnvironmentMapInOpenGL::~RRIlluminationEnvironmentMapInOpenGL()
{
	delete texture;
}

} // namespace
