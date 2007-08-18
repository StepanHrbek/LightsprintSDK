#pragma once

#include <cassert>
#include <map>
#include "RRObjectFilter.h"

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

	virtual const RRMaterial* getTriangleMaterial(unsigned t) const
	{
		const RRMaterial* custom = original->getTriangleMaterial(t);
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
			convertToPhysical(out);
		}
	}
	void convertToPhysicalFactor(RRColor& factor) const
	{
		RRColor tmp1 = RRColor(0.5f)*factor;
		RRColor tmp2 = RRColor(0.5f);
		scaler->getPhysicalScale(tmp1);
		scaler->getPhysicalScale(tmp2);
		factor = tmp1/tmp2;
	}
	void convertToPhysicalFactor(RRReal& factor) const
	{
		RRColor factor3 = RRColor(factor);
		convertToPhysicalFactor(factor3);
		factor = factor3[0];
	}
	void convertToPhysical(RRMaterial& physical) const // input=custom, output=physical
	{
		RR_ASSERT(scaler);
		convertToPhysicalFactor(physical.diffuseReflectance);
		scaler->getPhysicalScale(physical.diffuseEmittance);
		convertToPhysicalFactor(physical.specularReflectance);
		convertToPhysicalFactor(physical.specularTransmittance);
		physical.validate();
	}
	void convertToPhysical(const RRMaterial& custom, RRMaterial& physical) const
	{
		physical = custom;
		convertToPhysical(physical);
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
			const RRMaterial* custom = original->getTriangleMaterial(i);
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
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
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
