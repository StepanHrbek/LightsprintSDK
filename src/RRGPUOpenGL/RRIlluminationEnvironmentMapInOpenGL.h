// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONENVIRONMENTMAPINOPENGL_H
#define RRILLUMINATIONENVIRONMENTMAPINOPENGL_H

#include "RRIllumination.h"
#include "DemoEngine/Texture.h"

namespace rr_gl
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in environment map in OpenGL texture.
	//
	//! Uses OpenGL.
	//! Very simple to reimplement for example for Direct3D,
	//! you can look at .cpp in source directory.
	//!
	//! Thread safe: yes, multiple threads may call setValues at the same time.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationEnvironmentMapInOpenGL : public rr::RRIlluminationEnvironmentMap
	{
	public:
		RRIlluminationEnvironmentMapInOpenGL(const char* filenameMask, const char* cubeSideName[6]);
		virtual void setValues(unsigned size, rr::RRColorRGBF* irradiance);
		virtual void bindTexture();
		virtual bool save(const char* filename, const char* cubeSideName[6]);
		virtual ~RRIlluminationEnvironmentMapInOpenGL();
	private:
		friend class RRRealtimeRadiosityGL;
		de::Texture* texture;
	};

} // namespace

#endif
