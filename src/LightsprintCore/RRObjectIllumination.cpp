// --------------------------------------------------------------------------
// Illumination buffers for single object.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <boost/unordered_map.hpp>
#include "Lightsprint/RRIllumination.h"

namespace rr
{

class LayersMap : public boost::unordered_map<unsigned,RRBuffer*>
{
};

RRObjectIllumination::RRObjectIllumination()
{
	// static
	layersMap = new LayersMap;

	// dynamic
	envMapWorldCenter = RRVec3(0);
	envMapWorldRadius = 0;
	envMapObjectNumber = UINT_MAX; // truncation is ok, number is used only to penetrate object's own faces when shooting from center, so worst error is bad reflection on object 66000
	cachedCenter = RRVec3(0);
	cachedGatherSize = 0;
	cachedNumTriangles = 0;
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

	// static
	for (LayersMap::iterator i=layersMap->begin();i!=layersMap->end();i++)
	{
		delete i->second;
	}
	delete layersMap;
}

} // namespace
