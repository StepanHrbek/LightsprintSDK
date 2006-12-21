// --------------------------------------------------------------------------
// DemoEngine
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONENVIRONMENTMAPINOPENGL_H
#define RRILLUMINATIONENVIRONMENTMAPINOPENGL_H

#include <GL/glew.h>
#include "RRIllumination.h"
#include "DemoEngine/Texture.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in environment map in OpenGL texture.
	//
	//! Uses OpenGL.
	//! Very simple to reimplement for example for Direct3D,
	//! you can look at .cpp in source directory.
	//
	//////////////////////////////////////////////////////////////////////////////

	class DE_API RRIlluminationEnvironmentMapInOpenGL : public RRIlluminationEnvironmentMap
	{
	public:
		RRIlluminationEnvironmentMapInOpenGL();
		virtual void setValues(unsigned size, RRColorRGBA8* irradiance);
		virtual void setValues(unsigned size, RRColorRGBF* irradiance);
		virtual void bindTexture();
		virtual ~RRIlluminationEnvironmentMapInOpenGL();
	private:
		Texture* texture;
	};

} // namespace

#endif
