// --------------------------------------------------------------------------
// Static solver fragments that need exceptions enabled.
// Copyright (c) 2000-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "rrcore.h"

namespace rr
{

Object* Object::create(int _vertices,int _triangles)
{
	Object* o = new Object();
	unsigned size1 = (sizeof(RRVec3)*_vertices+sizeof(Triangle)*_triangles);
	unsigned size2 = sizeof(IVertex)*_vertices;
	if (size1+size2>10000000)
		RRReporter::report(INF1,"Memory taken by static solver: %d+%dMB\n",size1/1024/1024,size2/1024/1024);
	try
	{
		o->vertices = _vertices;
		o->triangles = _triangles;
		o->triangle=new Triangle[_triangles];
	}
	catch(std::bad_alloc e)
	{
		RR_SAFE_DELETE(o);
		RRReporter::report(ERRO,"Not enough memory, solver not created(1).\n");
	}
	return o;
}

} // namespace
