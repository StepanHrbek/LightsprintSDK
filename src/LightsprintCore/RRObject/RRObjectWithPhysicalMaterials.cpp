// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <boost/unordered_map.hpp>
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
		// Commented out, it would open path for invalid materials into solver.
		//if (!scaler) return custom;
		// Otherwise we ignore inherited and read result from our faceGroups.
		// We sync with inherited only when update() is called.
		return RRObject::getTriangleMaterial(t,light,receiver);
	}
	virtual void getPointMaterial(unsigned t, RRVec2 uv, RRPointMaterial& out, const RRScaler* _scaler = NULL) const
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
			out = *physicalMaterial;
			inherited->getPointMaterial(t,uv,out,scaler);
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
		physical.copyFrom(custom);
		physical.convertToPhysicalScale(scaler);
	}
	virtual void update(bool& aborting)
	{
		faceGroups = inherited->faceGroups;
		// Commented out, it would open path for invalid materials into solver.
		//if (!scaler) return;
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
	if (!scaler)
	{
		// Commented out, it would open path for invalid materials into solver.
		//return this;
	}
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

} // namespace
