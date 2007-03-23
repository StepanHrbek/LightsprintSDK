// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#include <windows.h>
#include <GL/glew.h>
#include "Lightsprint/DemoEngine/Program.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"
#include "Lightsprint/RRGPUOpenGL.h"

#define SAFE_DELETE(a)       {delete a;a=NULL;}

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

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(const char* filenameMask, const char* cubeSideName[6])
{
	if(!numInstances++) InitializeCriticalSection(&criticalSection);
	// creates cube map
	if(filenameMask)
		texture = de::Texture::load(filenameMask,cubeSideName,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	else
		texture = de::Texture::create(NULL,1,1,true,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
}


void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, rr::RRColorRGBF* irradiance)
{
	EnterCriticalSection(&criticalSection);
	texture->reset(size,size,de::Texture::TF_RGBF,(unsigned char*)irradiance,false);
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(unsigned side=0; side<6; side++)
	{
		//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, GL_RGBA8, size, size, GL_RGBA, GL_UNSIGNED_BYTE, &irradiance[size*size*side].color);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGB,size,size,0,GL_RGB,GL_FLOAT,&irradiance[size*size*side].x);
	}
	LeaveCriticalSection(&criticalSection);
}

rr::RRColorRGBF RRIlluminationEnvironmentMapInOpenGL::getValue(const rr::RRVec3& direction) const
{
	float tmp[4];
	texture->getPixel(direction[0],direction[1],direction[2],tmp);
	return rr::RRColorRGBF(tmp[0],tmp[1],tmp[2]);
};

void RRIlluminationEnvironmentMapInOpenGL::bindTexture()
{
	texture->bindTexture();
}

bool RRIlluminationEnvironmentMapInOpenGL::save(const char* filename, const char* cubeSideName[6])
{
	return texture->save(filename,cubeSideName);
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
	return new RRIlluminationEnvironmentMapInOpenGL(NULL,NULL);
}

rr::RRIlluminationEnvironmentMap* RRRealtimeRadiosityGL::loadIlluminationEnvironmentMap(const char* filenameMask, const char* cubeSideName[6])
{
	RRIlluminationEnvironmentMapInOpenGL* illum = new RRIlluminationEnvironmentMapInOpenGL(filenameMask,cubeSideName);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
