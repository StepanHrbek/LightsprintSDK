#include "RRIllumination.h"

namespace rr
{

void RRIlluminationEnvironmentMap::renderPixels(const IlluminatedPixel* ip, unsigned numPixels)
{
	for(unsigned i=0;i<numPixels;i++) renderPixel(ip[i]);
}

} // namespace
