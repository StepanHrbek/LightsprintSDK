// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include "RRIlluminationEnvironmentMapInMemory.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInMemory

RRIlluminationEnvironmentMapInMemory::RRIlluminationEnvironmentMapInMemory(const char* filename, const char* cubeSideName[6], bool flipV, bool flipH)
{
	texture = de::Texture::loadM(filename,cubeSideName,flipV,flipH);
}

RRIlluminationEnvironmentMapInMemory::RRIlluminationEnvironmentMapInMemory(unsigned width)
{
	texture = de::Texture::createM(NULL,width,width,true,de::Texture::TF_RGBF);
}

void RRIlluminationEnvironmentMapInMemory::setValues(unsigned size, const RRColorRGBF* irradiance)
{
	texture->reset(size,size,de::Texture::TF_RGBF,(const unsigned char*)irradiance,false);
}

RRColorRGBF RRIlluminationEnvironmentMapInMemory::getValue(const RRVec3& direction) const
{
	float tmp[4];
	texture->getPixel(direction[0],direction[1],direction[2],tmp);
	return rr::RRColorRGBF(tmp[0],tmp[1],tmp[2]);
}

void RRIlluminationEnvironmentMapInMemory::bindTexture() const
{
	texture->bindTexture();
}

bool RRIlluminationEnvironmentMapInMemory::save(const char* filenameMask, const char* cubeSideName[6])
{
	return texture->save(filenameMask,cubeSideName);
}

RRIlluminationEnvironmentMapInMemory::~RRIlluminationEnvironmentMapInMemory()
{
	delete texture;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMap

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::create(unsigned width)
{
	return new RRIlluminationEnvironmentMapInMemory(width);
}

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::load(const char* filename, const char* cubeSideName[6], bool flipV, bool flipH)
{
	RRIlluminationEnvironmentMapInMemory* environment = new RRIlluminationEnvironmentMapInMemory(filename,cubeSideName,flipV,flipH);
	if(!environment->texture)
		SAFE_DELETE(environment);
	return environment;
}

} // namespace
