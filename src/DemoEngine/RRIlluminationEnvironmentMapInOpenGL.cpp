#include <cstdio>
#include "DemoEngine/RRIlluminationEnvironmentMapInOpenGL.h"
#include "DemoEngine/Program.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInOpenGL

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(unsigned asize)
{
	texture = Texture::create(NULL,asize,asize,true,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
}

void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, RRColorRGBA8* irradiance)
{
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(unsigned i=0; i<6; i++) 
	{
		gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, GL_RGBA8, size, size, GL_RGBA, GL_UNSIGNED_BYTE, &irradiance[size*size*i].color);
		/*
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0,                  //level
			GL_RGBA8,           //internal format
			size,               //width
			size,               //height
			0,                  //border
			GL_RGBA,            //format
			GL_UNSIGNED_BYTE,   //type
			&irradiance[size*size*i].color); // pixel data
		*/
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
