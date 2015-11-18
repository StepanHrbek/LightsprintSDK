// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Mesh adapter that filters out vertices.
// --------------------------------------------------------------------------

#include "RRMeshLessVertices.h"

namespace rr
{

int __cdecl comparePositionX(const void* elem1, const void* elem2)
{
	RRMesh::Vertex& v1 = **(RRMesh::Vertex**)elem1;
	RRMesh::Vertex& v2 = **(RRMesh::Vertex**)elem2;
	if (v1[0]<v2[0]) return -1;
	if (v1[0]>v2[0]) return +1;
	/*if (v1[1]<v2[1]) return -1;
	if (v1[1]>v2[1]) return +1;
	if (v1[2]<v2[2]) return -1;
	if (v1[2]>v2[2]) return +1;*/
	return 0;
}

int __cdecl comparePositionNormal(const void* elem1, const void* elem2)
{
	RRMesh::Vertex& v1 = **(RRMesh::Vertex**)elem1;
	RRMesh::Vertex& v2 = **(RRMesh::Vertex**)elem2;
	if (v1[0]<v2[0]) return -1;
	if (v1[0]>v2[0]) return +1;
	if (v1[1]<v2[1]) return -1;
	if (v1[1]>v2[1]) return +1;
	if (v1[2]<v2[2]) return -1;
	if (v1[2]>v2[2]) return +1;
	if (v1[3]<v2[3]) return -1;
	if (v1[3]>v2[3]) return +1;
	if (v1[4]<v2[4]) return -1;
	if (v1[4]>v2[4]) return +1;
	if (v1[5]<v2[5]) return -1;
	if (v1[5]>v2[5]) return +1;
	return 0;
}

} //namespace
