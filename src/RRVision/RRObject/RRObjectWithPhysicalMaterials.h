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
	virtual const RRMaterial* getTriangleMaterial(unsigned t) const
	{
		if(!scaler || cache.find(t)==cache.end())
		{
			return original->getTriangleMaterial(t);
		}
		return &cache.find(t)->second;
	}
	void convertToPhysicalFactor(RRColor& factor)
	{
		RRColor tmp1 = RRColor(0.5f)*factor;
		RRColor tmp2 = RRColor(0.5f);
		scaler->getPhysicalScale(tmp1);
		scaler->getPhysicalScale(tmp2);
		factor = tmp1/tmp2;
	}
	void convertToPhysicalFactor(RRReal& factor)
	{
		RRColor factor3 = RRColor(factor);
		convertToPhysicalFactor(factor3);
		factor = factor3[0];
	}
	void convertToPhysical(const RRMaterial& custom, RRMaterial& physical)
	{
		assert(scaler);
		physical = custom;
		convertToPhysicalFactor(physical.diffuseReflectance);
		scaler->getPhysicalScale(physical.diffuseEmittance);
		convertToPhysicalFactor(physical.specularReflectance);
		convertToPhysicalFactor(physical.specularTransmittance);
		physical.validate();
	}
	virtual void update()
	{
		if(!scaler) return;
		cache.erase(cache.begin(),cache.end());
		assert(original->getCollider());
		assert(original->getCollider()->getMesh());
		unsigned numTriangles = original->getCollider()->getMesh()->getNumTriangles();
		for(unsigned i=0;i<numTriangles;i++)
		{
			if(cache.find(i)!=cache.end())
			{
				const RRMaterial* custom = original->getTriangleMaterial(i);
				RRMaterial physical;
				convertToPhysical(*custom,physical);
				cache.insert(Pair(i,physical));
			}
		}
	}

	// filter
	virtual const RRCollider* getCollider() const
	{
		return original->getCollider();
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		return original->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		return original->getTriangleMapping(t,out);
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
	typedef std::pair<unsigned,RRMaterial> Pair;
	typedef std::map<unsigned,RRMaterial> Cache;
	Cache cache;
};

}; // namespace
