// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Illumination buffers for single object.
// --------------------------------------------------------------------------

#include <unordered_map>
#include "Lightsprint/RRIllumination.h"

namespace rr
{

class LayersMap : public std::unordered_map<unsigned,RRBuffer*>
{
};

RRObjectIllumination::RRObjectIllumination()
{
	// static
	layersMap = new LayersMap;

	// dynamic
	envMapWorldCenter = RRVec3(0);
	envMapWorldRadius = 0;
	cachedCenter = RRVec3(0);
	cachedGatherSize = 0;
	cachedNumTriangles = 0;
	cachedTriangleNumbers = nullptr;
}

RRBuffer*& RRObjectIllumination::getLayer(unsigned layerNumber)
{
	LayersMap::iterator i = layersMap->find(layerNumber);
	if (i!=layersMap->end()) return i->second;
	return (*layersMap)[layerNumber] = nullptr;
}

RRBuffer* RRObjectIllumination::getLayer(unsigned layerNumber) const
{
	LayersMap::iterator i = layersMap->find(layerNumber);
	if (i!=layersMap->end()) return i->second;
	return nullptr;
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
