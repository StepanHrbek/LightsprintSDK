// --------------------------------------------------------------------------
// Offline lightmap filter.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef LIGHTMAPFILTER_H
#define LIGHTMAPFILTER_H

#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

// no scaling, operates on physical-scale data
class LightmapFilter
{
public:
	LightmapFilter(unsigned _width, unsigned _height);
	unsigned getWidth() const {return width;}
	unsigned getHeight() const {return height;}
	void renderTexelPhysical(const unsigned uv[2], const RRVec4& colorPhysical);
	unsigned getNumRenderedTexels() const {return numRenderedTexels;}
	RRVec4* getFilteredPhysical(const RRDynamicSolver::FilteringParameters* _params);
	~LightmapFilter();
private:
	unsigned width;
	unsigned height;
	RRVec4* renderedTexelsPhysical;
	unsigned numRenderedTexels;
};

}; // namespace

#endif // LIGHTMAPFILTER_H

