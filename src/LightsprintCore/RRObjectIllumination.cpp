// --------------------------------------------------------------------------
// Illumination buffers for single object.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <boost/unordered_map.hpp>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/RRCollider.h" // ray6

namespace rr
{

class LayersMap : public boost::unordered_map<unsigned,rr::RRBuffer*>
{
};

RRObjectIllumination::RRObjectIllumination()
{
	// static
	layersMap = new LayersMap;

	// dynamic
	diffuseEnvMap = NULL;
	specularEnvMap = NULL;
	gatherEnvMapSize = 16;
	envMapWorldCenter = RRVec3(0);
	envMapWorldRadius = 0;
	envMapObjectNumber = UINT_MAX;
	cachedCenter = RRVec3(0);
	cachedGatherSize = 0;
	cachedTriangleNumbers = NULL;
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

RRObjectIllumination::~RRObjectIllumination()
{
	// dynamic
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
