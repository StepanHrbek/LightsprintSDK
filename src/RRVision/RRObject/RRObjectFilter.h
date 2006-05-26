#pragma once

#include <assert.h>
#include "RRVision.h"

namespace rrVision
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectFilter

class RRObjectFilter : public RRObjectImporter
{
public:
	RRObjectFilter(RRObjectImporter* object)
	{
		base = object;
	}
	virtual const rrCollider::RRCollider* getCollider() const
	{
		return base->getCollider();
	}
	virtual unsigned getTriangleSurface(unsigned t) const
	{
		return base->getTriangleSurface(t);
	}
	virtual const RRSurface* getSurface(unsigned s) const
	{
		return base->getSurface(s);
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		return base->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		return base->getTriangleMapping(t,out);
	}
	virtual void getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		return base->getTriangleAdditionalMeasure(t,measure,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return base->getWorldMatrix();
	}
	virtual const RRMatrix3x4* getInvWorldMatrix()
	{
		return base->getInvWorldMatrix();
	}

protected:
	RRObjectImporter* base;
};

}; // namespace
