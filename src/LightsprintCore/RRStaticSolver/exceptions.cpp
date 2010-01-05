// --------------------------------------------------------------------------
// Static solver fragments that need exceptions enabled.
// Copyright (c) 2000-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "rrcore.h"

namespace rr
{


IVertex *Object::newIVertex()
{
	if (IVertexPoolItemsUsed>=IVertexPoolItems) 
	{
		IVertex *old=IVertexPool;
		unsigned newIVertexPoolItems=RR_MIN(RR_MAX(IVertexPoolItems,128)*2,32768);
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
