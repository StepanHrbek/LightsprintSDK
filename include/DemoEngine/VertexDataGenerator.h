#ifndef VERTEXDATAGENERATOR_H
#define VERTEXDATAGENERATOR_H


//////////////////////////////////////////////////////////////////////////////
//
// vertex data generator
//
// (can we replace this by channel in RRChanneledData?)

class VertexDataGenerator
{
public:
	virtual ~VertexDataGenerator() {};
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) = 0; // vertexIndex=0..2
};

#endif
