#include "RRIlluminationPixelBufferInOpenGL.h"

namespace rr
{

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::createInOpenGL(unsigned width, unsigned height)
{
	return new RRIlluminationPixelBufferInOpenGL(width,height);
}

} // namespace
