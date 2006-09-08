#include "RRIlluminationPixelBufferInMemory.h"
#include "swraster.h"

namespace rr
{

#define CLAMP(a,min,max) (a)=(((a)<(min))?min:(((a)>(max)?(max):(a))))

template <class Color>
void RRIlluminationPixelBufferInMemory<Color>::renderTriangle(const IlluminatedTriangle& it)
{
	raster_VERTEX vertex[3];
	raster_POLYGON polygon[3];
	for(unsigned i=0;i<3;i++)
	{
		vertex[i].sx = CLAMP(it.iv[i].texCoord[0],0,1)*width;
		vertex[i].sy = CLAMP(it.iv[i].texCoord[1],0,1)*height;
		vertex[i].u = (it.iv[i].measure[0]+it.iv[i].measure[1]+it.iv[i].measure[2])/3;//!!!
		assert(vertex[i].sx>=0);
		assert(vertex[i].sx<=width);
		assert(vertex[i].sy>=0);
		assert(vertex[i].sy<=height);
	}
	polygon[0].point = &vertex[0];
	polygon[0].next = &polygon[1];
	polygon[1].point = &vertex[1];
	polygon[1].next = &polygon[2];
	polygon[2].point = &vertex[2];
	polygon[2].next = NULL;
	raster_LGouraud(polygon,(raster_COLOR*)pixels,width);
}

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::createInSystemMemory(unsigned width, unsigned height)
{
	return new RRIlluminationPixelBufferInMemory<RRColorRGBA8>(width,height);
}

} // namespace
