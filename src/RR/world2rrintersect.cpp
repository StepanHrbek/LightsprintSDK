#include <assert.h>
#include <stdio.h>
#include <string.h>
//#include "surface.h"
#include "world2rrintersect.h"

#define DBG(a) a //!!!

WorldObjectImporter::WorldObjectImporter(OBJECT* aobject)
{
	object=aobject;
	assert(object);
	for(int i=0;i<object->vertex_num;i++) object->vertex[i].id=i;
}

WorldObjectImporter::~WorldObjectImporter()
{
}

unsigned WorldObjectImporter::getNumVertices()
{
	assert(object);
	return object->vertex_num;
}

void WorldObjectImporter::getVertex(unsigned i, float& x, float& y, float& z)
{
	assert(object);
	assert(i<object->vertex_num);
	x=object->vertex[i].x;
	y=object->vertex[i].y;
	z=object->vertex[i].z;
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
