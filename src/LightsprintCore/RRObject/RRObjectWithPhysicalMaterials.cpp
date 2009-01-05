// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RRObjectWithPhysicalMaterials.h"
#include <set> // generateRandomCamera
#include <cfloat> // _finite

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObjectWithPhysicalMaterials* RRObject::createObjectWithPhysicalMaterials(const RRScaler* scaler, bool& aborting)
{
	if (!this)
	{
		return NULL;
	}
	RRObjectWithPhysicalMaterials* result = new RRObjectWithPhysicalMaterialsImpl(this,scaler,aborting);
	if (aborting)
	{
		// Delete or not to delete?
		//   Delete to return NULL.
		//   Don't delete to return working, just not updated copy.
		// Both are equally efficient.
		// Working copy would be safer for users that don't check pointers for NULL.
		// However NULL is probably what user expects.
		RR_SAFE_DELETE(result);
	}
	return result;
}


// This function logically belongs to RRObject.cpp
// We put it here where exceptions are enabled.
// Enabling them in RRObject.cpp would be expensive.
void RRObject::generateRandomCamera(RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist) const
{
	if (!this)
	{
empty_scene:
		_pos = RRVec3(0);
		_dir = RRVec3(1,0,0);
		_maxdist = 10;
		return;
	}
	const RRMesh* mesh = getCollider()->getMesh();
	unsigned numVertices = mesh->getNumVertices();
	if (!numVertices)
	{
		goto empty_scene;
	}
	rr::RRVec3 mini,maxi,center;
	mesh->getAABB(&mini,&maxi,&center);
	_maxdist = (maxi-mini).length();
	if (!_maxdist) _maxdist = 10;
	unsigned bestNumFaces = 0;
	RRRay* ray = RRRay::create();
	std::set<unsigned> hitTriangles;
	for (unsigned i=0;i<20;i++)
	{
		// generate random pos+dir
		RRVec3 pos;
		mesh->getVertex(rand()%numVertices,pos);
		for (unsigned j=0;j<3;j++)
			if (!_finite(pos[j]))
				pos[j] = mini[j] + (maxi[j]-mini[j])*(rand()/float(RAND_MAX));
		RRVec3 dir = (center-pos);
		if (dir==RRVec3(0)) dir = RRVec3(1,0,0);
		dir.normalize();
		pos += dir*_maxdist*0.1f;

		// measure quality (=number of unique triangles hit by 100 rays)
		hitTriangles.clear();
		for (unsigned j=0;j<100;j++)
		{
			RRVec3 rayDir = ( dir + RRVec3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),rand()/float(RAND_MAX))-RRVec3(0.5f) ).normalized();
			ray->rayOrigin = pos;
			ray->rayDirInv[0] = 1/rayDir[0];
			ray->rayDirInv[1] = 1/rayDir[1];
			ray->rayDirInv[2] = 1/rayDir[2];
			ray->rayLengthMax = _maxdist;
			ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::TEST_SINGLESIDED;
			if (getCollider()->intersect(ray))
				hitTriangles.insert(ray->hitTriangle);
		}
		if (hitTriangles.size()>=bestNumFaces)
		{
			bestNumFaces = (unsigned)hitTriangles.size();
			_pos = pos;
			_dir = dir;
		}
	}
	delete ray;
}

} // namespace
