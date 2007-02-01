// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "RRGPUOpenGL.h"

namespace rr_gl
{

rr::RRIlluminationVertexBuffer* createIlluminationVertexBuffer(unsigned numVertices)
{
	return rr::RRIlluminationVertexBuffer::createInSystemMemory(numVertices);
}

void detectDirectIllumination(class rr::RRRealtimeRadiosity* solver)
{
	//...
}

}; // namespace
