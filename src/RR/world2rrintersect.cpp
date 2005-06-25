#include <assert.h>
#include "world2rrintersect.h"

WorldObjectImporter::WorldObjectImporter(OBJECT* aobject)
{
	object=aobject;
	assert(object);
	for(int i=0;i<object->vertex_num;i++) object->vertex[i].id=i;
}

WorldObjectImporter::~WorldObjectImporter()
{
}

unsigned WorldObjectImporter::getNumVertices() const
{
	assert(object);
	return object->vertex_num;
}

float* WorldObjectImporter::getVertex(unsigned i) const
{
	assert(object);
	assert(i<object->vertex_num);
	return &object->vertex[i].x;
}

unsigned WorldObjectImporter::getNumTriangles() const
{
	assert(object);
	assert(object->face_num);
	return object->face_num;
}

void WorldObjectImporter::getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2) const
{
	assert(object);
	assert(i<object->face_num);
	v0=object->face[i].vertex[0]->id;
	v1=object->face[i].vertex[1]->id;
	v2=object->face[i].vertex[2]->id;
}

void WorldObjectImporter::getTriangleSRL(unsigned i, TriangleSRL* t) const
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
