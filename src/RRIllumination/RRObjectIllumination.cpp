#include <map>
#include "Lightsprint/RRIllumination.h"

typedef std::map<unsigned,rr::RRObjectIllumination::Channel*> ChannelsType;
#define channels ((ChannelsType*)hiddenChannels)

namespace rr
{

RRObjectIllumination::RRObjectIllumination(unsigned anumPreImportVertices)
{
	numPreImportVertices = anumPreImportVertices;
	hiddenChannels = new ChannelsType;
}

RRObjectIllumination::Channel* RRObjectIllumination::getChannel(unsigned channelIndex)
{
	ChannelsType::iterator i = channels->find(channelIndex);
	if(i!=channels->end()) return i->second;
	Channel* tmp = new Channel();
	(*channels)[channelIndex] = tmp;
	return tmp;
}

const RRObjectIllumination::Channel* RRObjectIllumination::getChannel(unsigned channelIndex) const
{
	ChannelsType::iterator i = channels->find(channelIndex);
	if(i!=channels->end()) return i->second;
	Channel* tmp = new Channel();
	(*channels)[channelIndex] = tmp;
	return tmp;
}

unsigned RRObjectIllumination::getNumPreImportVertices()
{
	return numPreImportVertices;
}

RRObjectIllumination::~RRObjectIllumination()
{
	while(channels->begin()!=channels->end())
	{
		delete channels->begin()->second;
		channels->erase(channels->begin());
	}
	delete channels;
}

} // namespace
