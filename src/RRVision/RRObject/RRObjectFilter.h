#pragma once

#include <assert.h>
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
	virtual unsigned getTriangleSurface(unsigned t) const
	{
		return inherited->getTriangleSurface(t);
	}
	virtual const RRSurface* getSurface(unsigned s) const
	{
		return inherited->getSurface(s);
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		return inherited->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		return inherited->getTriangleMapping(t,out);
	}
	virtual void getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const
	{
		return inherited->getTriangleAdditionalMeasure(t,measure,out);
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
