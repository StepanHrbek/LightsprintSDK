#include "RRMeshLessVertices.h"

namespace rrCollider
{

int compareXyz(const void* elem1, const void* elem2)
{
	RRMeshImporter::Vertex& v1 = **(RRMeshImporter::Vertex**)elem1;
	RRMeshImporter::Vertex& v2 = **(RRMeshImporter::Vertex**)elem2;
	if(v1[0]<v2[0]) return -1;
	if(v1[0]>v2[0]) return +1;
	/*if(v1[1]<v2[1]) return -1;
	if(v1[1]>v2[1]) return +1;
	if(v1[2]<v2[2]) return -1;
	if(v1[2]>v2[2]) return +1;*/
	return 0;
}

} //namespace
