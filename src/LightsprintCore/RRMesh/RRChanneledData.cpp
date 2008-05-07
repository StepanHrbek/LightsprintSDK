// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRMesh.h"
#include <cassert>

namespace rr
{

void RRChanneledData::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	// legal query for presence of channel
	if(numItems) *numItems = 0;
	if(itemSize) *itemSize = 0;
}

bool RRChanneledData::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	// how to handle request to read channel unsupported by adapter?
	// a) warning, rev 1-1691
	//LIMITED_TIMES(1,RRReporter::report(WARN,"getChannelData: Unsupported channel %x requested.\n",channelId));
	// b) no warning, rev 1692-inf
	// our renderer reads even non-existing channels
	//  because channel may exist only for part of multiobject
	//  and it's expensive to verify channel's presence for each triangle
	return false;
}

} //namespace
