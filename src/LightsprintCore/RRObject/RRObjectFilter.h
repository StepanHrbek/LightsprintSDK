// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Object filter.
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

	virtual RRCollider* getCollider() const
	{
		return inherited->getCollider();
	}
	virtual void setCollider(RRCollider* _collider)
	{
		inherited->setCollider(_collider);
	}
	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		return inherited->getTriangleMaterial(t,light,receiver);
	}
	virtual void getPointMaterial(unsigned t, RRVec2 uv, const RRColorSpace* colorSpace, bool interpolated, RRPointMaterial& out) const
	{
		inherited->getPointMaterial(t,uv,colorSpace,interpolated,out);
	}
	virtual void getTriangleLod(unsigned t, LodInfo& out) const
	{
		inherited->getTriangleLod(t,out);
	}
	virtual const RRMatrix3x4Ex* getWorldMatrix() const
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
