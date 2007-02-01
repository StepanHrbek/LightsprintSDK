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
		RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight, const char* pathToShaders);
		virtual void renderBegin();
		virtual void renderTriangle(const IlluminatedTriangle& it);
		//virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles);
		virtual void renderEnd();
		virtual void bindTexture();
		virtual ~RRIlluminationPixelBufferInOpenGL();
	private:
		de::Texture* texture;
		// state backup
		bool rendering;
		GLint viewport[4];
		GLboolean depthTest, depthMask;
		GLfloat clearcolor[4];
		// static
		static unsigned numInstances; // number of living instances
	};

} // namespace

#endif
