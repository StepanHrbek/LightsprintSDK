#pragma once

#include <cassert>
#include "RRVision.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectFilter

class RRObjectFilter : public RRObject
{
public:
	RRObjectFilter(RRObject* object)
	{
		inherited = object;
	}
	virtual const RRCollider* getCollider() const
	{
		return inherited->getCollider();
	}
	virtual const RRMaterial* getTriangleMaterial(unsigned t) const
	{
		return inherited->getTriangleMaterial(t);
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		return inherited->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		return inherited->getTriangleMapping(t,out);
	}
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		return inherited->getTriangleIllumination(t,measure,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return inherited->getWorldMatrix();
	}
	virtual const RRMatrix3x4* getInvWorldMatrix()
	{
		return inherited->getInvWorldMatrix();
	}

protected:
	RRObject* inherited;
};

}; // namespace
