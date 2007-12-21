#pragma once

#include "RRObjectFilter.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectWithIllumination

/*
proc ukladam zrovna irradiance (ne flux, ne incident)?

solvery zajima exiting flux
renderer s texturou zajima irradiance, renderer bez textury vyjimecne exitance

nekonzistence vznika kdyz u cerneho materialu ulozim outgoing a ptam se na incoming
-> ukladat incoming
nekonzistance vznika kdyz u degenerata ulozim flux a ptam se na irradiance
-> ukladat irradiance

RRObjectWithIllumination tedy vse co dostane prevede na irradiance
*/

class RRObjectWithIllumination : public RRObjectFilter
{
public:
	RRObjectWithIllumination(RRObject* _inherited, const RRScaler* _scaler)
		: RRObjectFilter(_inherited)
	{
		scaler = _scaler;
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
	virtual ~RRObjectWithIllumination() 
	{
		delete[] triangleInfo;
	}

	virtual bool setTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRVec3 power)
	{
		RR_ASSERT(power[0]>=0);
		RR_ASSERT(power[1]>=0);
		RR_ASSERT(power[2]>=0);
		if(t>=numTriangles)
		{
			RR_ASSERT(0);
			return false;
		}
		if(measure.flux)
		{
			power = triangleInfo[t].area ? power / triangleInfo[t].area : RRVec3(0);
		}
		if(measure.scaled && scaler)
		{
			// scaler applied on density, not flux
			scaler->getPhysicalScale(power);
		}
		if(measure.exiting)
		{
			const RRMaterial* s = getTriangleMaterial(t,NULL,NULL);
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
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRVec3& out) const
	{
		if(t>=numTriangles)
		{
			RR_ASSERT(0);
			out = RRVec3(0);
			return;
		}
		RRVec3 power = triangleInfo[t].irradiance;
		if(measure.exiting)
		{
			const RRMaterial* s = getTriangleMaterial(t,NULL,NULL);
			if(!s)
			{
				RR_ASSERT(0);
				out = RRVec3(0);
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

private:
	struct TriangleInfo
	{
		RRVec3 irradiance; // always in physical scale
		RRReal area;
	};
	const RRScaler* scaler;
	unsigned numTriangles;
	TriangleInfo* triangleInfo;
};

}; // namespace
