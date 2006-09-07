#include "RRIlluminationPixelBufferInMemory.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in OpenGL texture.
	//
	//! Uses OpenGL rasterizer.
	//! Depends on OpenGL, can't be used on Xbox and Xbox 360.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationPixelBufferInOpenGL : public RRIlluminationPixelBufferInMemory<RRColorRGBA8>
	{
	public:
		RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight)
			: RRIlluminationPixelBufferInMemory<RRColorRGBA8>(awidth,aheight)
		{
		}
	};

} // namespace
