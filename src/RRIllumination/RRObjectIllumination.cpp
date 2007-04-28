#include <map>
#include "Lightsprint/RRIllumination.h"

typedef std::map<unsigned,rr::RRObjectIllumination::Layer*> LayersType;
#define layers ((LayersType*)hiddenLayers)

namespace rr
{

RRObjectIllumination::RRObjectIllumination(unsigned anumPreImportVertices)
{
	numPreImportVertices = anumPreImportVertices;
	hiddenLayers = new LayersType;
}

RRObjectIllumination::Layer* RRObjectIllumination::getLayer(unsigned layerNumber)
{
	LayersType::iterator i = layers->find(layerNumber);
	if(i!=layers->end()) return i->second;
	Layer* tmp = new Layer();
	(*layers)[layerNumber] = tmp;
	return tmp;
}

const RRObjectIllumination::Layer* RRObjectIllumination::getLayer(unsigned layerNumber) const
{
	LayersType::iterator i = layers->find(layerNumber);
	if(i!=layers->end()) return i->second;
	Layer* tmp = new Layer();
	(*layers)[layerNumber] = tmp;
	return tmp;
}

unsigned RRObjectIllumination::getNumPreImportVertices() const
{
	return numPreImportVertices;
}

RRObjectIllumination::~RRObjectIllumination()
{
	while(layers->begin()!=layers->end())
	{
		delete layers->begin()->second;
		layers->erase(layers->begin());
	}
	delete layers;
}

} // namespace
