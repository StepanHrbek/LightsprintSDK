// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006
// --------------------------------------------------------------------------

#include <windows.h>
#include "DemoEngine/Program.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInOpenGL

// Many Lightsprint functions are parallelized internally,
// but demos are singlethreaded for simplicity, so this code 
// is never run in multiple threads and critical section is not needed.
// But it could get handy later.
CRITICAL_SECTION criticalSection; // global critical section for all instances, never calls GL from 2 threads at once
unsigned numInstances = 0;

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL()
{
	if(!numInstances++) InitializeCriticalSection(&criticalSection);
	// creates cube map
	texture = Texture::create(NULL,1,1,true,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
}

void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, RRColorRGBA8* irradiance)
{
	EnterCriticalSection(&criticalSection);
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(unsigned side=0; side<6; side++)
	{
		//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, GL_RGBA8, size, size, GL_RGBA, GL_UNSIGNED_BYTE, &irradiance[size*size*side].color);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,&irradiance[size*size*side].color);
	}
	LeaveCriticalSection(&criticalSection);
}

void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, RRColorRGBF* irradiance)
{
	EnterCriticalSection(&criticalSection);
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(unsigned side=0; side<6; side++)
	{
		//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, GL_RGBA8, size, size, GL_RGBA, GL_UNSIGNED_BYTE, &irradiance[size*size*side].color);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGB,size,size,0,GL_RGB,GL_FLOAT,&irradiance[size*size*side].x);
	}
	LeaveCriticalSection(&criticalSection);
}

void RRIlluminationEnvironmentMapInOpenGL::bindTexture()
{
	texture->bindTexture();
}

RRIlluminationEnvironmentMapInOpenGL::~RRIlluminationEnvironmentMapInOpenGL()
{
	delete texture;
	if(!--numInstances) DeleteCriticalSection(&criticalSection);
}

} // namespace
