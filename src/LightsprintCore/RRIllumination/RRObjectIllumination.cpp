#include <map>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/RRCollider.h" // ray6

typedef std::map<unsigned,rr::RRObjectIllumination::Layer*> LayersType;
#define layers ((LayersType*)hiddenLayers)

namespace rr
{

RRObjectIllumination::RRObjectIllumination(unsigned anumPreImportVertices)
{
	// static
	numPreImportVertices = anumPreImportVertices;
	hiddenLayers = new LayersType;

	// dynamic
	diffuseEnvMap = NULL;
	specularEnvMap = NULL;
	gatherEnvMapSize = 16;
	diffuseEnvMapSize = 4;
	specularEnvMapSize = 16;
	cachedCenter = RRVec3(0);
	cachedGatherSize = 0;
	cachedTriangleNumbers = NULL;
	ray6 = RRRay::create(6);
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
	// dynamic
	delete[] ray6;
	delete[] cachedTriangleNumbers;
	delete specularEnvMap;
	delete diffuseEnvMap;

	// static
	while(layers->begin()!=layers->end())
	{
		delete layers->begin()->second;
		layers->erase(layers->begin());
	}
	delete layers;
}

} // namespace
