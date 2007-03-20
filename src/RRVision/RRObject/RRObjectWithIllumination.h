#pragma once

#include <cassert>
#include "Lightsprint/RRVision.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectWithIllumination
/*
vision zajima exiting flux
renderer s texturou zajima irradiance, renderer bez textury vyjimecne exitance

fast pocita incident flux
slow pocita exitance

nekonzistence vznika kdyz u cerneho materialu ulozim outgoing a ptam se na incoming
-> ukladat incoming
nekonzistance vznika kdyz u degenerata ulozim flux a ptam se na irradiance
-> ukladat irradiance

RRObjectWithIlluminationImpl tedy vse co dostane prevede na irradiance
*/

class RRObjectWithIlluminationImpl : public RRObjectWithIllumination
{
public:
	RRObjectWithIlluminationImpl(RRObject* aoriginal, const RRScaler* ascaler)
	{
		original = aoriginal;
		scaler = ascaler;
		RR_ASSERT(original);
		RR_ASSERT(getCollider());
		RR_ASSERT(getCollider()->getMesh());
		numTriangles = getCollider()->getMesh()->getNumTriangles();
		triangleInfo = new TriangleInfo[numTriangles];
		for(unsigned i=0;i<numTriangles;i++)
		{
			triangleInfo[i].irradiance[0] = 0;
			triangleInfo[i].irradiance[1] = 0;
			triangleInfo[i].irradiance[2] = 0;
			triangleInfo[i].area = getCollider()->getMesh()->getTriangleArea(i);
		}
	}
	virtual ~RRObjectWithIlluminationImpl() 
	{
		delete[] triangleInfo;
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

	virtual bool setTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor power)
	{
		if(t>=numTriangles)
		{
			RR_ASSERT(0);
			return false;
		}
		if(measure.flux)
		{
			power = triangleInfo[t].area ? power / triangleInfo[t].area : RRColor(0);
		}
		if(measure.scaled && scaler)
		{
			// scaler applied on density, not flux
			scaler->getPhysicalScale(power);
		}
		if(measure.exiting)
		{
			const RRMaterial* s = getTriangleMaterial(t);
			if(!s)
			{
				RR_ASSERT(0);
				return false;
			}
			for(unsigned c=0;c<3;c++)
				// diffuse applied on physical scale, not custom scale
				power[c] = (s->diffuseReflectance[c]) ? power[c] / s->diffuseReflectance[c] : 0;
		}
		triangleInfo[t].irradiance = power;
		return true;
	}
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		if(t>=numTriangles)
		{
			RR_ASSERT(0);
			out = RRColor(0);
			return;
		}
		RRColor power = triangleInfo[t].irradiance;
		if(measure.exiting)
		{
			const RRMaterial* s = getTriangleMaterial(t);
			if(!s)
			{
				RR_ASSERT(0);
				out = RRColor(0);
				return;
			}
			// diffuse applied on physical scale, not custom scale
			power *= s->diffuseReflectance;
		}
		if(measure.scaled && scaler)
		{
			// scaler applied on density, not flux
			scaler->getCustomScale(power);
		}
		if(measure.flux)
		{
			power *= triangleInfo[t].area;
		}
		out = power;
	}

	// filter
	virtual const RRCollider* getCollider() const
	{
		return original->getCollider();
	}
	virtual const RRMaterial* getTriangleMaterial(unsigned t) const
	{
		return original->getTriangleMaterial(t);
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
	struct TriangleInfo
	{
		RRColor irradiance; // always in physical scale
		RRReal area;
	};
	RRObject* original;
	const RRScaler* scaler;
	unsigned numTriangles;
	TriangleInfo* triangleInfo;
};

}; // namespace
