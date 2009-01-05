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
	RRObjectWithPhysicalMaterialsImpl(RRObject* aoriginal, const RRScaler* ascaler, bool& aborting)
	{
		original = aoriginal;
		scaler = ascaler;
		update(aborting);
	}
	virtual ~RRObjectWithPhysicalMaterialsImpl() 
	{
	}

	virtual const RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		const RRMaterial* custom = original->getTriangleMaterial(t,light,receiver);
		if (!scaler || !custom)
		{
			return custom;
		}
		const Cache::const_iterator i = cache.find(custom);
		if (i==cache.end())
		{
			// all materials should be stuffed into cache in update
			// this could happen only if underlying RRObject returns
			//  pointer different to what it returned in update
			//  (= breaks rules)
			RR_ASSERT(0);
			return custom;
		}
		return &i->second;
	}
	virtual void getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out, RRScaler* _scaler = NULL) const
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
		if (!scaler) return;
		cache.erase(cache.begin(),cache.end());
		RR_ASSERT(original->getCollider());
		RR_ASSERT(original->getCollider()->getMesh());
		unsigned numTriangles = original->getCollider()->getMesh()->getNumTriangles();
		for (unsigned i=0;i<numTriangles;i++)
		{
			if (!aborting)
			{
				const RRMaterial* custom = original->getTriangleMaterial(i,NULL,NULL);
				if (custom && cache.find(custom)==cache.end())
				{
					RRMaterial physical;
					convertToPhysical(*custom,physical);
					cache.insert(Pair(custom,physical));
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

private:
	RRObject* original;
	const RRScaler* scaler;
	typedef std::pair<const RRMaterial*,RRMaterial> Pair;
	typedef std::map<const RRMaterial*,RRMaterial> Cache;
	Cache cache;
};

}; // namespace
