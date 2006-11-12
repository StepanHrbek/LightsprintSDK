// --------------------------------------------------------------------------
// DemoEngine
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#include "RRIllumination.h"
#include "DemoEngine/Texture.h"
#include <GL/glew.h>

namespace rr
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

	class RR_API RRIlluminationPixelBufferInOpenGL : public RRIlluminationPixelBuffer
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
		Texture* texture;
		// state backup
		bool rendering;
		GLint viewport[4];
		GLboolean depthTest, depthMask;
		GLfloat clearcolor[4];
		// static
		static unsigned numInstances; // number of living instances
	};

} // namespace
