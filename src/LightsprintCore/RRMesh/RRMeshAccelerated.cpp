// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RRMeshFilter.h"

#include "../RRMathPrivate.h"
#include <map> // findGroundLevel

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Mesh with local copy of TrangleBase and TriangleNormals for faster access

class RRMeshAccelerated : public RRMeshFilter
{
public:
	RRMeshAccelerated(const RRMesh* _inherited)
		: RRMeshFilter(_inherited)
	{
		RR_ASSERT(_inherited);
		numTriangles = _inherited ? _inherited->getNumTriangles() : 0;
		data = new TriangleData[numTriangles];
		for(unsigned t=0;t<numTriangles;t++)
		{
			_inherited->getTriangleBody(t,data[t].body);
			_inherited->getTriangleNormals(t,data[t].normals);
		}
	}
	virtual ~RRMeshAccelerated()
	{
		delete[] data;
	}
	virtual void         getTriangleBody(unsigned t, TriangleBody& out) const
	{
		if(t<numTriangles)
			out = data[t].body;
	}
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		if(t<numTriangles)
			out = data[t].normals;
	}

protected:
	struct TriangleData
	{
		TriangleBody body;
		TriangleNormals normals;
	};
	unsigned        numTriangles;
	TriangleData*   data;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

const RRMesh* RRMesh::createAccelerated() const
{
	try
	{
		return this ? new RRMeshAccelerated(this) : NULL;
	}
	catch(std::bad_alloc e)
	{
		RRReporter::report(ERRO,"Not enough memory, mesh not accelerated.\n");
		return this;
	}
}

// calculates triangle area from triangle vertices
static RRReal calculateArea(RRVec3 v0, RRVec3 v1, RRVec3 v2)
{
	RRReal a = size2(v1-v0);
	RRReal b = size2(v2-v0);
	RRReal c = size2(v2-v1);
	//return sqrt(2*b*c+2*c*a+2*a*b-a*a-b*b-c*c)/4;
	RRReal d = 2*b*c+2*c*a+2*a*b-a*a-b*b-c*c;
	return (d>0) ? sqrt(d)*0.25f : 0; // protection against evil rounding error
}

// this function logically belongs to RRMesh.cpp,
// we put it here where exceptions are enabled
// enabling exceptions in RRMesh.cpp would cause slow down
RRReal RRMesh::findGroundLevel() const
{
	unsigned numTriangles = getNumTriangles();
	if(numTriangles)
	{
		typedef std::map<RRReal,RRReal> YToArea;
		YToArea yToArea;
		for(unsigned t=0;t<numTriangles;t++)
		{
			RRMesh::TriangleBody tb;
			getTriangleBody(t,tb);
			if(tb.side1.y==0 && tb.side2.y==0 && orthogonalTo(tb.side1,tb.side2).y>0) // planar and facing up
			{
				RRReal area = calculateArea(tb.side1,tb.side2,tb.side2-tb.side1);
				YToArea::iterator i = yToArea.find(tb.vertex0.y);
				if(i==yToArea.end())
					yToArea[tb.vertex0.y]=area;
				else
					yToArea[tb.vertex0.y]+=area;
			}
		}
		YToArea::iterator best = yToArea.begin();
		for(YToArea::iterator i=yToArea.begin();i!=yToArea.end();i++)
		{
			if(i->second>best->second) best = i;
		}
		return best->first;
	}
	else
	{
		return 0;
	}
}

}; // namespace
