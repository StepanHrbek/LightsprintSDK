// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <map>
#include "RRObjectFilter.h"
#include "Lightsprint/RRLight.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectWithPhysicalMaterials

class RRObjectWithPhysicalMaterialsImpl : public RRObjectWithPhysicalMaterials
{
public:
	RRObjectWithPhysicalMaterialsImpl(RRObject* _original, const RRScaler* _scaler, bool& _aborting)
	{
		original = _original;
		name = original->name;
		scaler = _scaler;
		update(_aborting);
	}
	virtual ~RRObjectWithPhysicalMaterialsImpl() 
	{
	}

	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		// Original may return NULL to disable lighting/shadowing for selected light/caster/receiver.
		RRMaterial* custom = original->getTriangleMaterial(t,light,receiver);
		// We honour original NULL decision.
		if (!custom)
			return custom;
		// Fast path if no scaler was provided.
		if (!scaler)
			return custom;
		// Otherwise we ignore original and read result from our faceGroups.
		// We sync with original only when update() is called.
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
			out = *physicalMaterial;
			original->getPointMaterial(t,uv,out,scaler);
		}
		else
		{
			// Material was NULL? It's illegal, error in 'original'.
			//! Let's protect system and return defaults.
			out.reset(false);
		}
	}
	void convertToPhysical(const RRMaterial& custom, RRMaterial& physical) const
	{
		physical = custom;
		physical.convertToPhysicalScale(scaler);
	}
	virtual void update(bool& aborting)
	{
		faceGroups = original->faceGroups;
		if (!scaler) return;
		cache.erase(cache.begin(),cache.end());
		for (unsigned g=0;g<original->faceGroups.size();g++)
		{
			if (faceGroups[g].material)
			{
				Cache::iterator i = cache.find(faceGroups[g].material);
				if (i!=cache.end())
					faceGroups[g].material = &i->second;
				else
				{
					RRMaterial physical;
					convertToPhysical(*faceGroups[g].material,physical);
					faceGroups[g].material = &(cache[faceGroups[g].material] = physical);
				}
			}
		}
	}

	// filter
	virtual const RRCollider* getCollider() const
	{
		return original->getCollider();
	}
	virtual void getTriangleLod(unsigned t, LodInfo& out) const
	{
		original->getTriangleLod(t,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return original->getWorldMatrix();
	}
	virtual void setWorldMatrix(const RRMatrix3x4* _worldMatrix)
	{
		original->setWorldMatrix(_worldMatrix);
	}

private:
	RRObject* original;
	const RRScaler* scaler;
	typedef std::pair<const RRMaterial*,RRMaterial> Pair;
	typedef std::map<const RRMaterial*,RRMaterial> Cache;
	Cache cache;
};

}; // namespace
