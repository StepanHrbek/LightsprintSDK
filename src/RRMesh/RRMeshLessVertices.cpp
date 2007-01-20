#include "RRMeshLessVertices.h"

namespace rr
{

int __cdecl compareXyz(const void* elem1, const void* elem2)
{
	RRMesh::Vertex& v1 = **(RRMesh::Vertex**)elem1;
	RRMesh::Vertex& v2 = **(RRMesh::Vertex**)elem2;
	if(v1[0]<v2[0]) return -1;
	if(v1[0]>v2[0]) return +1;
	/*if(v1[1]<v2[1]) return -1;
	if(v1[1]>v2[1]) return +1;
	if(v1[2]<v2[2]) return -1;
	if(v1[2]>v2[2]) return +1;*/
	return 0;
}

} //namespace
