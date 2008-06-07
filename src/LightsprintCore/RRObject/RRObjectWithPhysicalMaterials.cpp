// --------------------------------------------------------------------------
// Object adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RRObjectWithPhysicalMaterials.h"
#include <set> // generateRandomCamera

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObjectWithPhysicalMaterials* RRObject::createObjectWithPhysicalMaterials(const RRScaler* scaler)
{
	/*
	//!!! zatim nejde zoptimalizovat, protoze update() neni v RRObjectu
	if(!scaler)
	{
		RR_ASSERT(0);
		return this;
	}
	*/
	return this ? new RRObjectWithPhysicalMaterialsImpl(this,scaler) : NULL;
}


// This function logically belongs to RRObject.cpp
// We put it here where exceptions are enabled.
// Enabling them in RRObject.cpp would be expensive.
void RRObject::generateRandomCamera(RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist)
{
	const RRMesh* mesh = getCollider()->getMesh();
	unsigned numVertices = mesh->getNumVertices();
	if(!numVertices)
	{
		_pos = RRVec3(0);
		_dir = RRVec3(1,0,0);
		_maxdist = 1;
		return;
	}
	rr::RRVec3 mini,maxi,center;
	mesh->getAABB(&mini,&maxi,&center);
	_maxdist = (maxi-mini).sum();
	unsigned bestNumFaces = 0;
	RRRay* ray = RRRay::create();
	std::set<unsigned> hitTriangles;
	for(unsigned i=0;i<20;i++)
	{
		// generate random pos+dir
		RRVec3 pos;
		mesh->getVertex(numVertices*rand()/(RAND_MAX+1),pos);
		RRVec3 dir = (center-pos).normalized();
		pos += dir*_maxdist*0.1f;

		// measure quality (=number of unique triangles hit by 100 rays)
		hitTriangles.clear();
		for(unsigned j=0;j<100;j++)
		{
			RRVec3 rayDir = ( dir + RRVec3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),rand()/float(RAND_MAX))-RRVec3(0.5f) ).normalized();
			ray->rayOrigin = pos;
			ray->rayDirInv[0] = 1/rayDir[0];
			ray->rayDirInv[1] = 1/rayDir[1];
			ray->rayDirInv[2] = 1/rayDir[2];
			ray->rayLengthMax = _maxdist;
			ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::TEST_SINGLESIDED;
			if(getCollider()->intersect(ray))
				hitTriangles.insert(ray->hitTriangle);
		}
		if(hitTriangles.size()>=bestNumFaces)
		{
			bestNumFaces = hitTriangles.size();
			_pos = pos;
			_dir = dir;
		}
	}
	delete ray;
}

} // namespace
