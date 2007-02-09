// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONPIXELBUFFERINOPENGL_H
#define RRILLUMINATIONPIXELBUFFERINOPENGL_H

#include "RRIllumination.h"
#include "DemoEngine/Texture.h"

namespace rr_gl
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in OpenGL texture.
	//
	//! Uses OpenGL rasterizer.
	//!
	//! Requires files: lightmap_filter.vp, lightmap_filter.fp.
	//! Pass pathToShaders with ending slash to constructor OR "" for default path.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationPixelBufferInOpenGL : public rr::RRIlluminationPixelBuffer
	{
	public:
		//! Creates rr::RRIlluminationPixelBuffer implemented using OpenGL 2.0.
		RRIlluminationPixelBufferInOpenGL(unsigned width, unsigned height, const char* pathToShaders, bool swapChannels);
		virtual void renderBegin();
		virtual void renderTriangle(const IlluminatedTriangle& it);
		//virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles);
		virtual void renderTexel(const unsigned uv[2], const rr::RRColorRGBAF& color);
		virtual void renderEnd();
		virtual unsigned getWidth() const;
		virtual unsigned getHeight() const;
		virtual void bindTexture();
		virtual ~RRIlluminationPixelBufferInOpenGL();
	private:
		de::Texture* texture;
		bool rendering;
		rr::RRColorRGBA8* renderedTexels;
		bool swapChannels;
		// state backup
		GLint viewport[4];
		GLboolean depthTest, depthMask;
		GLfloat clearcolor[4];
		// static
		static unsigned numInstances; // number of living instances
	};

} // namespace

#endif
