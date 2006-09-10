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
		RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight);
		virtual void renderBegin();
		virtual void renderTriangle(const IlluminatedTriangle& it);
		//virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles);
		virtual void renderEnd();
		virtual void bindTexture();
		virtual ~RRIlluminationPixelBufferInOpenGL();
	private:
		Texture* texture;
		bool usePBO;
		// state backup
		bool rendering;
		GLint viewport[4];
		GLboolean depthTest, scissorTest, depthMask;
		// static
		static unsigned numInstances; // number of living instances
	};

} // namespace
