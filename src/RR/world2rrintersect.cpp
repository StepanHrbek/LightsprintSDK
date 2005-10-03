#include <assert.h>
#include "world2rrintersect.h"

WorldMeshImporter::WorldMeshImporter(OBJECT* aobject)
{
	object=aobject;
	assert(object);
	for(int i=0;i<object->vertex_num;i++) object->vertex[i].id=i;
}

WorldMeshImporter::~WorldMeshImporter()
{
}

unsigned WorldMeshImporter::getNumVertices() const
{
	assert(object);
	return object->vertex_num;
}

void WorldMeshImporter::getVertex(unsigned i, Vertex& out) const
{
	assert(object);
	assert(i<object->vertex_num);
	out[0] = object->vertex[i].x;
	out[1] = object->vertex[i].y;
	out[2] = object->vertex[i].z;
}

unsigned WorldMeshImporter::getNumTriangles() const
{
	assert(object);
	assert(object->face_num);
	return object->face_num;
}

void WorldMeshImporter::getTriangle(unsigned i, Triangle& out) const
{
	assert(object);
	assert(i<object->face_num);
	out[0]=object->face[i].vertex[0]->id;
	out[1]=object->face[i].vertex[1]->id;
	out[2]=object->face[i].vertex[2]->id;
}

void WorldMeshImporter::getTriangleBody(unsigned i, TriangleBody& out) const
{
	assert(object);
	assert(i<object->face_num);
	unsigned v0=object->face[i].vertex[0]->id;
	unsigned v1=object->face[i].vertex[1]->id;
	unsigned v2=object->face[i].vertex[2]->id;
	out.vertex0[0] = object->vertex[v0].x;
	out.vertex0[1] = object->vertex[v0].y;
	out.vertex0[2] = object->vertex[v0].z;
	out.side1[0] = object->vertex[v1].x-object->vertex[v0].x;
	out.side1[1] = object->vertex[v1].y-object->vertex[v0].y;
	out.side1[2] = object->vertex[v1].z-object->vertex[v0].z;
	out.side2[0] = object->vertex[v2].x-object->vertex[v0].x;
	out.side2[1] = object->vertex[v2].y-object->vertex[v0].y;
	out.side2[2] = object->vertex[v2].z-object->vertex[v0].z;
}
