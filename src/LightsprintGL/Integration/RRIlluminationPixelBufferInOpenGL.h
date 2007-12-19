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
		RRIlluminationPixelBufferInOpenGL(const char* filename, unsigned width, unsigned height, const char* pathToShaders);
		virtual void renderBegin();
		virtual void renderTexel(const unsigned uv[2], const rr::RRColorRGBAF& color);
		virtual void renderEnd(bool preferQualityOverSpeed);
		virtual unsigned getWidth() const;
		virtual unsigned getHeight() const;
		virtual void bindTexture() const;
		virtual bool save(const char* filename);
		virtual ~RRIlluminationPixelBufferInOpenGL();
	private:
		friend class RRDynamicSolverGL;
		Texture* texture;
		bool rendering;
		bool renderTriangleProgramSet;
		rr::RRColorRGBAF* renderedTexels;
		unsigned numRenderedTexels;
		// state backup
		GLint viewport[4];
		GLboolean depthTest, depthMask;
		GLfloat clearcolor[4];
		// static
		static unsigned numInstances; // number of living instances
	};

} // namespace

#endif
