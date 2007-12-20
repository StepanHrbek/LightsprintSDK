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

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		inherited->getChannelSize(channelId,numItems,itemSize);
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		return inherited->getChannelData(channelId,itemIndex,itemData,itemSize);
	}

	virtual const RRCollider* getCollider() const
	{
		return inherited->getCollider();
	}
	virtual const RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		return inherited->getTriangleMaterial(t,light,receiver);
	}
	virtual void getPointMaterial(unsigned t,RRVec2 uv,RRMaterial& out) const
	{
		inherited->getPointMaterial(t,uv,out);
	}
	virtual void getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRVec3& out) const
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
