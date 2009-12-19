// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <boost/unordered_map.hpp>
#include <set> // generateRandomCamera
#include <cfloat> // _finite in generateRandomCamera
#include "RRObjectFilter.h"
#include "Lightsprint/RRLight.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectWithPhysicalMaterials

class RRObjectWithPhysicalMaterials : public RRObjectFilter
{
public:
	RRObjectWithPhysicalMaterials(RRObject* _inherited, const RRScaler* _scaler, bool& _aborting)
		: RRObjectFilter(_inherited)
	{
		name = _inherited->name;
		scaler = _scaler;
		update(_aborting);
	}
	virtual ~RRObjectWithPhysicalMaterials() 
	{
		// delete physical materials
		for (Cache::iterator i=cache.begin();i!=cache.end();++i)
		{
			delete i->second;
		}
	}

	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		// Inherited may return NULL to disable lighting/shadowing for selected light/caster/receiver.
		RRMaterial* custom = inherited->getTriangleMaterial(t,light,receiver);
		// We honour inherited NULL decision.
		if (!custom)
			return custom;
		// Fast path if no scaler was provided.
		if (!scaler)
			return custom;
		// Otherwise we ignore inherited and read result from our faceGroups.
		// We sync with inherited only when update() is called.
		return RRObject::getTriangleMaterial(t,light,receiver);
	}
	virtual void getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out, const RRScaler* _scaler = NULL) const
	{
		// No one else is allowed to set scaler, it's our job.
		// We also don't expect user would create chain with two instances of this filter.
		RR_ASSERT(!_scaler);
		// Read cached values.
		const RRMaterial* physicalMaterial = getTriangleMaterial(t,NULL,NULL);
		if (physicalMaterial)
		{
			// This is the only place in whole SDK where out is prefilled (in physical scale)
			// and scaler set.
			
			// slow path - full copy with name allocation and texture addrefing
			//             this code is executed in threads, so addrefing would have to use locks,
			//             then delete would be called, needing more locks for lowering refcount
			//out = *physicalMaterial;
			//inherited->getPointMaterial(t,uv,out,scaler);

			// fast path - shallow copy including pointers (inherited->getPointMaterial needs textures), then clear pointers
			//             some material subclasses delete textures, others do not, therefore we must not copy textures
			memcpy(&out,physicalMaterial,sizeof(out));
			inherited->getPointMaterial(t,uv,out,scaler);
			out.diffuseReflectance.texture = NULL;
			out.specularReflectance.texture = NULL;
			out.diffuseEmittance.texture = NULL;
			out.specularTransmittance.texture = NULL;
			memset(&out.name,0,sizeof(out.name));
		}
		else
		{
			// Material was NULL? It's illegal, error in 'inherited'.
			//! Let's protect system and return defaults.
			out.reset(false);
		}
	}
	void convertToPhysical(const RRMaterial& custom, RRMaterial& physical) const
	{
		// assignment is disabled
		//physical = custom;
		// use memcpy
		//  fix name but keep copied texture pointers, they are not (yet) deleted in destructor
		memcpy(&physical,&custom,sizeof(physical));
		memset(&physical.name,0,sizeof(physical.name)); // fix name (prevent double delete)
		physical.name = custom.name;
		
		physical.convertToPhysicalScale(scaler);
	}
	virtual void update(bool& aborting)
	{
		faceGroups = inherited->faceGroups;
		if (!scaler) return;
		// delete physical materials
		for (Cache::iterator i=cache.begin();i!=cache.end();++i)
		{
			delete i->second;
		}
		cache.clear();
		// allocate physical materials
		for (unsigned g=0;g<inherited->faceGroups.size();g++)
		{
			if (faceGroups[g].material)
			{
				Cache::iterator i = cache.find(faceGroups[g].material);
				if (i!=cache.end())
					faceGroups[g].material = i->second;
				else
				{
					RRMaterial* physical = new RRMaterial;
					convertToPhysical(*faceGroups[g].material,*physical);
					faceGroups[g].material = cache[faceGroups[g].material] = physical;
				}
			}
		}
	}

private:
	const RRScaler* scaler;
	typedef boost::unordered_map<const RRMaterial*,RRMaterial*> Cache;
	Cache cache;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObject* RRObject::createObjectWithPhysicalMaterials(const RRScaler* scaler, bool& aborting)
{
	if (!this)
	{
		return NULL;
	}
	RRObject* result = new RRObjectWithPhysicalMaterials(this,scaler,aborting);
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
	RRVec3 mini,maxi,center;
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
