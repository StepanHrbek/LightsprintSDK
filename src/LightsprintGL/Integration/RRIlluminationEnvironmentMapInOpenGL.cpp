// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/Program.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInOpenGL

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL()
{
	// creates cube map
	texture = Texture::create(NULL,1,1,true,Texture::TF_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	deleteTexture = true;
}

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(Texture* cube)
{
	// adapts cube map
	texture = cube;
	deleteTexture = false;
}

RRIlluminationEnvironmentMapInOpenGL::RRIlluminationEnvironmentMapInOpenGL(const char* filenameMask, const char* cubeSideName[6], bool flipV, bool flipH)
{
	// loads cube map
	texture = Texture::load(filenameMask,cubeSideName,flipV,flipH,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	deleteTexture = true;
}

void RRIlluminationEnvironmentMapInOpenGL::setValues(unsigned size, const rr::RRVec3* irradiance)
{
	// not thread safe
	// OpenGL context is thread specific, so calling this from other than master thread is usually error

	// intentionally converted to bytes here, because older cards can't interpolate floats
	texture->reset(size,size,Texture::TF_RGBF,(unsigned char*)irradiance,false);
}

rr::RRVec3 RRIlluminationEnvironmentMapInOpenGL::getValue(const rr::RRVec3& direction) const
{
	float tmp[4];
	texture->getPixel(direction[0],direction[1],direction[2],tmp);
	return rr::RRVec3(tmp[0],tmp[1],tmp[2]);
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
}


/////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolverGL

rr::RRIlluminationEnvironmentMap* RRDynamicSolverGL::createIlluminationEnvironmentMap()
{
	return new RRIlluminationEnvironmentMapInOpenGL();
}

rr::RRIlluminationEnvironmentMap* RRDynamicSolverGL::adaptIlluminationEnvironmentMap(Texture* cube)
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
