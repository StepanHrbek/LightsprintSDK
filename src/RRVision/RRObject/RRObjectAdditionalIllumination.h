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
		switch(measure)
		{
		case RM_INCIDENT_FLUX:
			triangleInfo[t].irradiance = triangleInfo[t].area ? power / triangleInfo[t].area : RRColor(0);
			break;
		case RM_IRRADIANCE:
			triangleInfo[t].irradiance = power;
			break;
		case RM_EXITING_FLUX:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				return false;
			}
			//triangleInfo[t].irradiance = power / triangleInfo[t].area / s->diffuseReflectance;
			for(unsigned c=0;c<3;c++)
				triangleInfo[t].irradiance[c] = (triangleInfo[t].area && s->diffuseReflectance[c]) ? power[c] / triangleInfo[t].area / s->diffuseReflectance[c] : 0;
			break;
			}
		case RM_EXITANCE:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				return false;
			}
			//triangleInfo[t].irradiance = power / s->diffuseReflectance;
			for(unsigned c=0;c<3;c++)
				triangleInfo[t].irradiance[c] = (s->diffuseReflectance[c]) ? power[c] / s->diffuseReflectance[c] : 0;
			break;
			}
		default:
			assert(0);
			return false;
		}
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
		switch(measure)
		{
		case RM_INCIDENT_FLUX:
			out = triangleInfo[t].irradiance * triangleInfo[t].area;
			break;
		case RM_IRRADIANCE:
			out = triangleInfo[t].irradiance;
			break;
		case RM_EXITING_FLUX:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				out = RRColor(0);
				return;
			}
			out = triangleInfo[t].irradiance * triangleInfo[t].area * s->diffuseReflectance;
			break;
			}
		case RM_EXITANCE:
			{
			const RRSurface* s = getSurface(getTriangleSurface(t));
			if(!s)
			{
				assert(0);
				out = RRColor(0);
				return;
			}
			out = triangleInfo[t].irradiance * s->diffuseReflectance;
			break;
			}
		default:
			assert(0);
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
		RRColor irradiance;
		RRReal area;
	};
	RRObject* original;
	unsigned numTriangles;
	TriangleInfo* triangleInfo;
};

}; // namespace
