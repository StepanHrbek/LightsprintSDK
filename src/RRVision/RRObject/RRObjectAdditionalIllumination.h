#pragma once

#include <assert.h>
#include "RRVision.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectAdditionalIllumination
/*
vision zajima exiting flux
renderer s texturou zajima irradiance, renderer bez textury vyjimecne exitance

fast pocita incident flux
slow pocita exitance

nekonzistence vznika kdyz u cerneho materialu ulozim outgoing a ptam se na incoming
-> ukladat incoming
nekonzistance vznika kdyz u degenerata ulozim flux a ptam se na irradiance
-> ukladat irradiance

RRObjectAdditionalIlluminationImpl tedy vse co dostane prevede na irradiance
*/

class RRObjectAdditionalIlluminationImpl : public RRObjectAdditionalIllumination
{
public:
	RRObjectAdditionalIlluminationImpl(RRObject* aoriginal)
	{
		original = aoriginal;
		assert(original);
		assert(getCollider());
		assert(getCollider()->getMesh());
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
	virtual ~RRObjectAdditionalIlluminationImpl() 
	{
		delete[] triangleInfo;
	}
	virtual bool setTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor power)
	{
		if(t>=numTriangles)
		{
			assert(0);
			return false;
		}
		if(measure.exiting)
		{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				return false;
			}
			for(unsigned c=0;c<3;c++)
				power[c] = (s->diffuseReflectance[c]) ? power[c] / s->diffuseReflectance[c] : 0;
		}
		if(measure.flux)
		{
			power = triangleInfo[t].area ? power / triangleInfo[t].area : RRColor(0);
		}
		assert(measure.scaled);
		triangleInfo[t].irradiance = power;
		return true;
	}
	virtual void getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		if(t>=numTriangles)
		{
			assert(0);
			out = RRColor(0);
			return;
		}
		RRColor power = triangleInfo[t].irradiance;
		if(measure.exiting)
		{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				out = RRColor(0);
				return;
			}
			power *= s->diffuseReflectance;
		}
		if(measure.flux)
		{
			power *= triangleInfo[t].area;
		}
		assert(measure.scaled);
		out = power;
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
	virtual const RRSurface* getSurface(unsigned s) const
	{
		return original->getSurface(s);
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		return original->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		return original->getTriangleMapping(t,out);
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
		RRColor irradiance; // always scaled
		RRReal area;
	};
	RRObject* original;
	unsigned numTriangles;
	TriangleInfo* triangleInfo;
};

}; // namespace
