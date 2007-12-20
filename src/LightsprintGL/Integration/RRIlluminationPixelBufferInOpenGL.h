// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONPIXELBUFFERINOPENGL_H
#define RRILLUMINATIONPIXELBUFFERINOPENGL_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/Texture.h"

namespace rr_gl
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in OpenGL texture.
	//
	//! Uses OpenGL rasterizer.
	//!
	//! Requires files:
	//!  lightmap_build.vs/fs, lightmap_filter.vs/fp.
	//! \n Pass pathToShaders with trailing slash to constructor.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationPixelBufferInOpenGL : public rr::RRIlluminationPixelBuffer
	{
	public:
		//! Creates rr::RRIlluminationPixelBuffer implemented using OpenGL 2.0.
		RRIlluminationPixelBufferInOpenGL(const char* filename, unsigned width, unsigned height);
		virtual void reset(const rr::RRVec4* data);
		virtual unsigned getWidth() const;
		virtual unsigned getHeight() const;
		virtual void bindTexture() const;
		virtual bool save(const char* filename);
		virtual ~RRIlluminationPixelBufferInOpenGL();
	private:
		friend class RRDynamicSolverGL;
		Texture* texture;
	};

} // namespace

#endif
