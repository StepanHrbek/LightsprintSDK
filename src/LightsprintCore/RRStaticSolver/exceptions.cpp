// --------------------------------------------------------------------------
// Static solver fragments that need exceptions enabled.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "rrcore.h"

namespace rr
{

Object* Object::create(int _vertices,int _triangles)
{
	Object* o = new Object();
	try
	{
		o->vertices = _vertices;
		o->triangles = _triangles;
		o->vertex=new RRVec3[_vertices];
		o->triangle=new Triangle[_triangles];
	}
	catch(std::bad_alloc e)
	{
		SAFE_DELETE(o);
		RRReporter::report(ERRO,"Not enough memory, solver not created.\n");
	}
	return o;
}

} // namespace
