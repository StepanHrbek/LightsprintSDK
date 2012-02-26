// --------------------------------------------------------------------------
// RRMesh minimalistic example.
//
// RRMesh is constructed from vertex buffer.
//
// Copyright (C) 2006-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRMesh.h"
#include <stdio.h>

using namespace rr;

int main()
{
	// simple vertex buffer used for demonstration
	float vertexArray[18] =
	{
		// triangle 0
		-1,0,0, // vertex 0
		1,0,0, // vertex 1
		0,1,0, // vertex 2
		// triangle 1
		1,0,0, // vertex 1
		-1,0,0, // vertex 0
		0,0,1, // vertex 3
	};

	// create mesh
	// note that vertex buffer is not duplicated,
	// RRMesh is only tiny wrapper around your data
	RRMesh* mesh = RRMesh::create(
		RRMesh::TRI_LIST, // our data represent triangle list
		RRMesh::FLOAT32,  // our coordinates are 32bit floats
		vertexArray,      // pointer to our data
		6,                // number of vertices
		3*sizeof(float)   // stride
		);

	// print mesh size
	printf("Mesh constructed:\n\n");
	printf("Triangles in mesh = %d\n",mesh->getNumTriangles());
	printf("Vertices in mesh = %d\n",mesh->getNumVertices());
	printf("\nPress enter to end...");
	fgetc(stdin);

	// cleanup
	delete mesh;

	return 0;
}
