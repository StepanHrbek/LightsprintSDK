#include <assert.h>
#include "world2rrintersect.h"

WorldObjectImporter::WorldObjectImporter(OBJECT* aobject)
{
	object=aobject;
	assert(object);
	for(int i=0;i<object->vertex_num;i++) object->vertex[i].id=i;
	fastSRL=true;
}

WorldObjectImporter::~WorldObjectImporter()
{
}

unsigned WorldObjectImporter::getNumVertices()
{
	assert(object);
	return object->vertex_num;
}

float* WorldObjectImporter::getVertex(unsigned i)
{
	assert(object);
	assert(i<object->vertex_num);
	return &object->vertex[i].x;
}

unsigned WorldObjectImporter::getNumTriangles()
{
	assert(object);
	return object->face_num;
}

void WorldObjectImporter::getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2, unsigned& si)
{
	assert(object);
	assert(i<object->face_num);
	v0=object->face[i].vertex[0]->id;
	v1=object->face[i].vertex[1]->id;
	v2=object->face[i].vertex[2]->id;
	si=object->face[i].material;
}

void WorldObjectImporter::getTriangleSRL(unsigned i, TriangleSRL* t)
{
	assert(object);
	assert(i<object->face_num);
	unsigned v0=object->face[i].vertex[0]->id;
	unsigned v1=object->face[i].vertex[1]->id;
	unsigned v2=object->face[i].vertex[2]->id;
	t->s[0] = object->vertex[v0].x;
	t->s[1] = object->vertex[v0].y;
	t->s[2] = object->vertex[v0].z;
	t->r[0] = object->vertex[v1].x-object->vertex[v0].x;
	t->r[1] = object->vertex[v1].y-object->vertex[v0].y;
	t->r[2] = object->vertex[v1].z-object->vertex[v0].z;
	t->l[0] = object->vertex[v2].x-object->vertex[v0].x;
	t->l[1] = object->vertex[v2].y-object->vertex[v0].y;
	t->l[2] = object->vertex[v2].z-object->vertex[v0].z;
}
