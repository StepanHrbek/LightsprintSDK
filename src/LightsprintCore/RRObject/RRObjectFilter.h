// --------------------------------------------------------------------------
// Object adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include <cassert>
#include "Lightsprint/RRObject.h"

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
	virtual const RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		return inherited->getTriangleMaterial(t,light,receiver);
	}
	virtual void getPointMaterial(unsigned t,RRVec2 uv,RRMaterial& out, const RRScaler* scaler = NULL) const
	{
		inherited->getPointMaterial(t,uv,out,scaler);
	}
	virtual void getTriangleLod(unsigned t, LodInfo& out) const
	{
		inherited->getTriangleLod(t,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return inherited->getWorldMatrix();
	}

protected:
	RRObject* inherited;
};

}; // namespace
