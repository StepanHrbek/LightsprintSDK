// --------------------------------------------------------------------------
// DemoEngine
// VertexDataGenerator, generates abstract data for vertex stream.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef VERTEXDATAGENERATOR_H
#define VERTEXDATAGENERATOR_H

#include "DemoEngine.h"


//////////////////////////////////////////////////////////////////////////////
//
// vertex data generator
//
// (can we replace this later by channel in RRChanneledData?)

class DE_API VertexDataGenerator
{
public:
	virtual ~VertexDataGenerator() {};
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) = 0; // vertexIndex=0..2
};

#endif
