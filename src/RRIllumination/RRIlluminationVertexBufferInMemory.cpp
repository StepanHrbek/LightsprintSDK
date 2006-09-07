#include "RRIlluminationVertexBufferInMemory.h"

namespace rr
{

RRIlluminationVertexBuffer* RRIlluminationVertexBuffer::createInSystemMemory(unsigned numVertices)
{
	return new RRIlluminationVertexBufferRGBFInMemory(numVertices);
}

} // namespace
