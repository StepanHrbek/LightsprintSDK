// --------------------------------------------------------------------------
// Object adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
	RRObjectWithPhysicalMaterialsImpl(RRObject* aoriginal, const RRScaler* ascaler)
	{
		original = aoriginal;
		scaler = ascaler;
		update();
	}
	virtual ~RRObjectWithPhysicalMaterialsImpl() 
	{
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		original->getChannelSize(channelId,numItems,itemSize);
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		return original->getChannelData(channelId,itemIndex,itemData,itemSize);
	}

	virtual const RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		const RRMaterial* custom = original->getTriangleMaterial(t,light,receiver);
		if(!scaler || !custom)
		{
			return custom;
		}
		const Cache::const_iterator i = cache.find(custom);
		if(i==cache.end())
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
	virtual void getPointMaterial(unsigned t,RRVec2 uv,RRMaterial& out) const
	{
		original->getPointMaterial(t,uv,out);
		if(scaler)
		{
			out.convertToPhysicalScale(scaler);
		}
	}
	void convertToPhysical(const RRMaterial& custom, RRMaterial& physical) const
	{
		physical = custom;
		physical.convertToPhysicalScale(scaler);
	}
	virtual void update()
	{
		if(!scaler) return;
		cache.erase(cache.begin(),cache.end());
		RR_ASSERT(original->getCollider());
		RR_ASSERT(original->getCollider()->getMesh());
		unsigned numTriangles = original->getCollider()->getMesh()->getNumTriangles();
		for(unsigned i=0;i<numTriangles;i++)
		{
			const RRMaterial* custom = original->getTriangleMaterial(i,NULL,NULL);
			if(custom && cache.find(custom)==cache.end())
			{
				RRMaterial physical;
				convertToPhysical(*custom,physical);
				cache.insert(Pair(custom,physical));
			}
		}
	}

	// filter
	virtual const RRCollider* getCollider() const
	{
		return original->getCollider();
	}
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRVec3& out) const
	{
		return original->getTriangleIllumination(t,measure,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return original->getWorldMatrix();
	}
	virtual const RRMatrix3x4* getInvWorldMatrix()
	{
		return original->getInvWorldMatrix();
	}

private:
	RRObject* original;
	const RRScaler* scaler;
	typedef std::pair<const RRMaterial*,RRMaterial> Pair;
	typedef std::map<const RRMaterial*,RRMaterial> Cache;
	Cache cache;
};

}; // namespace
