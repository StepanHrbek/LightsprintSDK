#pragma once

#include <cassert>
#include <map>
#include "RRObjectFilter.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectWithPhysicalSurfaces

class RRObjectWithPhysicalSurfacesImpl : public RRObjectWithPhysicalSurfaces
{
public:
	RRObjectWithPhysicalSurfacesImpl(RRObject* aoriginal, const RRScaler* ascaler)
	{
		original = aoriginal;
		scaler = ascaler;
		update();
	}
	virtual ~RRObjectWithPhysicalSurfacesImpl() 
	{
	}
	virtual const RRSurface* getSurface(unsigned s) const
	{
		if(!scaler || map.find(s)==map.end())
		{
			return original->getSurface(s);
		}
		return &map.find(s)->second;
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
	void convertToPhysical(const RRSurface& custom, RRSurface& physical)
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
		map.erase(map.begin(),map.end());
		assert(original->getCollider());
		assert(original->getCollider()->getMesh());
		unsigned numTriangles = original->getCollider()->getMesh()->getNumTriangles();
		for(unsigned i=0;i<numTriangles;i++)
		{
			unsigned s = original->getTriangleSurface(i);
			if(map.find(s)!=map.end())
			{
				const RRSurface* custom = original->getSurface(s);
				RRSurface physical;
				convertToPhysical(*custom,physical);
				map.insert(Pair(s,physical));
			}
		}
	}

	// filter
	virtual const RRCollider* getCollider() const
	{
		return original->getCollider();
	}
	virtual unsigned getTriangleSurface(unsigned t) const
	{
		return original->getTriangleSurface(t);
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
	typedef std::pair<unsigned,RRSurface> Pair;
	std::map<unsigned,RRSurface> map;
};

}; // namespace
