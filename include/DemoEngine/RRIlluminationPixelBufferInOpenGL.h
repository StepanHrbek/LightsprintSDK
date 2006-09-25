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
	//! Depends on OpenGL, can't be used on Xbox and Xbox 360.
	//! Warning: doesn't restore changed GL states.
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
