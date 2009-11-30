// --------------------------------------------------------------------------
// Illumination buffers for single object.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <map>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/RRCollider.h" // ray6

namespace rr
{

class LayersMap : public std::map<unsigned,rr::RRBuffer*>
{
};

RRObjectIllumination::RRObjectIllumination(unsigned anumPreImportVertices)
{
	// static
	numPreImportVertices = anumPreImportVertices;
	layersMap = new LayersMap;

	// dynamic
	envMapWorldCenter = RRVec3(0);
	diffuseEnvMap = NULL;
	specularEnvMap = NULL;
	gatherEnvMapSize = 16;
	cachedCenter = RRVec3(0);
	cachedGatherSize = 0;
	cachedTriangleNumbers = NULL;
	ray6 = RRRay::create(6);
}

RRBuffer*& RRObjectIllumination::getLayer(unsigned layerNumber)
{
	LayersMap::iterator i = layersMap->find(layerNumber);
	if (i!=layersMap->end()) return i->second;
	return (*layersMap)[layerNumber] = NULL;
}

RRBuffer* RRObjectIllumination::getLayer(unsigned layerNumber) const
{
	LayersMap::iterator i = layersMap->find(layerNumber);
	if (i!=layersMap->end()) return i->second;
	return NULL;
}

unsigned RRObjectIllumination::getNumPreImportVertices() const
{
	return this ? numPreImportVertices : 0;
}

RRObjectIllumination::~RRObjectIllumination()
{
	// dynamic
	delete[] ray6;
	delete[] cachedTriangleNumbers;
	delete specularEnvMap;
	delete diffuseEnvMap;

	// static
	for (LayersMap::iterator i=layersMap->begin();i!=layersMap->end();i++)
	{
		delete i->second;
	}
	delete layersMap;
}

} // namespace
