// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#include <windows.h>
#include <GL/glew.h>
#include "DemoEngine/Program.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"
#include "RRGPUOpenGL.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInOpenGL

// Many Lightsprint functions are parallelized internally,
// but demos are singlethreaded for simplicity, so this code 
// is never run in multiple threads and critical section is not needed (by demos).
// It could be important for future applications.
CRITICAL_SECTION criticalSection; // global critical section for all instances, never calls GL from 2 threads at once
unsigned numInstances = 0;

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL()
{
	if(!numInstances++) InitializeCriticalSection(&criticalSection);
	// creates cube map
	texture = de::Texture::create(NULL,1,1,true,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
}


void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, rr::RRColorRGBF* irradiance)
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

rr::RRColorRGBF RRIlluminationEnvironmentMapInOpenGL::getValue(const rr::RRVec3& direction)
{
	assert(0);
	return rr::RRColorRGBF(0);
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


/////////////////////////////////////////////////////////////////////////////
//
// RRGPUOpenGL

rr::RRIlluminationEnvironmentMap* RRRealtimeRadiosityGL::createIlluminationEnvironmentMap()
{
	return new RRIlluminationEnvironmentMapInOpenGL();
}

} // namespace
