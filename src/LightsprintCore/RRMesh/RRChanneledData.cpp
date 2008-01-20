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
	// legal, but shouldn't happen in well coded program
	LIMITED_TIMES(1,RRReporter::report(WARN,"getChannelData: Unsupported channel %x requested.\n",channelId));
	return false;
}

} //namespace
