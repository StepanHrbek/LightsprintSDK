// --------------------------------------------------------------------------
// Static solver fragments that need exceptions enabled.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "rrcore.h"

namespace rr
{


IVertex *Object::newIVertex()
{
	if(IVertexPoolItemsUsed>=IVertexPoolItems) 
	{
		IVertex *old=IVertexPool;
		unsigned newIVertexPoolItems=MIN(MAX(IVertexPoolItems,128)*2,32768);
		try
		{
			IVertexPool=new IVertex[newIVertexPoolItems];
		}
		catch(std::bad_alloc e)
		{
			RRReporter::report(ERRO,"Not enough memory, solver not created(2).\n");
			return NULL;
		}
		IVertexPoolItems=newIVertexPoolItems;
		IVertexPoolItemsUsed=1;
		IVertexPool->previousAllocBlock=old; // store pointer to old block on safe place in new block
	}
	return &IVertexPool[IVertexPoolItemsUsed++];
}


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
		RRReporter::report(ERRO,"Not enough memory, solver not created(1).\n");
	}
	return o;
}

} // namespace
