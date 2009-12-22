// --------------------------------------------------------------------------
// Object adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

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
		name = object->name;
		faceGroups = object->faceGroups;
	}

	virtual const RRCollider* getCollider() const
	{
		return inherited->getCollider();
	}
	virtual void setCollider(const RRCollider* _collider)
	{
		inherited->setCollider(_collider);
	}
	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		return inherited->getTriangleMaterial(t,light,receiver);
	}
	virtual void getPointMaterial(unsigned t,RRVec2 uv,RRPointMaterial& out, const RRScaler* scaler = NULL) const
	{
		inherited->getPointMaterial(t,uv,out,scaler);
	}
	virtual void getTriangleLod(unsigned t, LodInfo& out) const
	{
		inherited->getTriangleLod(t,out);
	}
	virtual const RRMatrix3x4* getWorldMatrix() const
	{
		return inherited->getWorldMatrix();
	}
	virtual void setWorldMatrix(const RRMatrix3x4* _worldMatrix)
	{
		return inherited->setWorldMatrix(_worldMatrix);
	}

protected:
	RRObject* inherited;
};

}; // namespace
