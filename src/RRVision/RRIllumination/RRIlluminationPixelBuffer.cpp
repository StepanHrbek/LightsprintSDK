#include "Lightsprint/RRIllumination.h"

namespace rr
{

void RRIlluminationPixelBuffer::renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles)
{
	for(unsigned i=0;i<numTriangles;i++) renderTriangle(it[i]);
}

} // namespace
