// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#include <windows.h>
#include <GL/glew.h>
#include "Lightsprint/DemoEngine/Program.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

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
	texture = de::Texture::create(NULL,1,1,true,de::Texture::TF_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	deleteTexture = true;
}

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(de::Texture* cube)
{
	if(!numInstances++) InitializeCriticalSection(&criticalSection);
	// adapts cube map
	texture = cube;
	deleteTexture = false;
}

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(const char* filenameMask, const char* cubeSideName[6], bool flipV, bool flipH)
{
	if(!numInstances++) InitializeCriticalSection(&criticalSection);
	// loads cube map
	texture = de::Texture::load(filenameMask,cubeSideName,flipV,flipH,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	deleteTexture = true;
}

void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, const rr::RRColorRGBF* irradiance)
{
	EnterCriticalSection(&criticalSection);
	// intentionally converted to bytes here, because older cards can't interpolate floats
	texture->reset(size,size,de::Texture::TF_RGBF,(unsigned char*)irradiance,false);
	LeaveCriticalSection(&criticalSection);
}

rr::RRColorRGBF RRIlluminationEnvironmentMapInOpenGL::getValue(const rr::RRVec3& direction) const
{
	float tmp[4];
	texture->getPixel(direction[0],direction[1],direction[2],tmp);
	return rr::RRColorRGBF(tmp[0],tmp[1],tmp[2]);
};

void RRIlluminationEnvironmentMapInOpenGL::bindTexture() const
{
	texture->bindTexture();
}

bool RRIlluminationEnvironmentMapInOpenGL::save(const char* filename, const char* cubeSideName[6])
{
	return texture->save(filename,cubeSideName);
}

RRIlluminationEnvironmentMapInOpenGL::~RRIlluminationEnvironmentMapInOpenGL()
{
	if(deleteTexture) delete texture;
	if(!--numInstances) DeleteCriticalSection(&criticalSection);
}


/////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolverGL

rr::RRIlluminationEnvironmentMap* RRDynamicSolverGL::createIlluminationEnvironmentMap()
{
	return new RRIlluminationEnvironmentMapInOpenGL();
}

rr::RRIlluminationEnvironmentMap* RRDynamicSolverGL::adaptIlluminationEnvironmentMap(de::Texture* cube)
{
	return cube ? new RRIlluminationEnvironmentMapInOpenGL(cube) : NULL;
}

rr::RRIlluminationEnvironmentMap* RRDynamicSolverGL::loadIlluminationEnvironmentMap(const char* filenameMask, const char* cubeSideName[6], bool flipV, bool flipH)
{
	const char* notset[6] = {"bk","ft","up","dn","rt","lf"};
	RRIlluminationEnvironmentMapInOpenGL* illum = new RRIlluminationEnvironmentMapInOpenGL(filenameMask,cubeSideName?cubeSideName:notset,flipV,flipH);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
