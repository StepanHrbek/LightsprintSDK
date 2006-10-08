// --------------------------------------------------------------------------
// DemoEngine
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006
// --------------------------------------------------------------------------

#include "RRIllumination.h"
#include "DemoEngine/Texture.h"
#include <GL/glew.h>

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in environment map in OpenGL texture.
	//
	//! Uses OpenGL.
	//! Depends on OpenGL, can't be used on Xbox and Xbox 360.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationEnvironmentMapInOpenGL : public RRIlluminationEnvironmentMap
	{
	public:
		RRIlluminationEnvironmentMapInOpenGL(unsigned asize);
		virtual void setValues(unsigned size, RRColorRGBA8* irradiance);
		virtual void bindTexture();
		virtual ~RRIlluminationEnvironmentMapInOpenGL();
	private:
		Texture* texture;
	};

} // namespace
