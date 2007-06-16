#include "RRIlluminationVertexBufferInMemory.h"

namespace rr
{

RRIlluminationVertexBuffer* RRIlluminationVertexBuffer::createInSystemMemory(unsigned numVertices)
{
	return new RRIlluminationVertexBufferRGBFInMemory(numVertices);
}

RRIlluminationVertexBuffer* RRIlluminationVertexBuffer::load(const char* filename, unsigned expectedNumVertices)
{
	RRIlluminationVertexBufferRGBFInMemory* tmp = new RRIlluminationVertexBufferRGBFInMemory(expectedNumVertices);
	if(!tmp->load(filename))
	{
		delete tmp;
		tmp = NULL;
	}
	return tmp;
}

} // namespace
