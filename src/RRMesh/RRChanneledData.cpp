#include "RRMesh.h"
#include <cassert>

namespace rr
{

void RRChanneledData::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	assert(0); // legal, but shouldn't happen in well coded program
	if(numItems) *numItems = 0;
	if(itemSize) *itemSize = 0;
}

bool RRChanneledData::getChannelData(unsigned channelId, unsigned itemIndex, void* item) const
{
	assert(0); // legal, but shouldn't happen in well coded program
	return false;
}

} //namespace
