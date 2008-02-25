// --------------------------------------------------------------------------
// Offline lightmap filter.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef LIGHTMAPFILTER_H
#define LIGHTMAPFILTER_H

#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

// no scaling, operates on raw unknown-scale data
class LightmapFilter
{
public:
	LightmapFilter(unsigned _width, unsigned _height);
	unsigned getWidth() {return width;}
	unsigned getHeight() {return height;}
	void renderTexel(const unsigned uv[2], const RRVec4& color);
	RRVec4* getFiltered(const RRDynamicSolver::FilteringParameters* _params);
	~LightmapFilter();
private:
	unsigned width;
	unsigned height;
	RRVec4* renderedTexels;
	unsigned numRenderedTexels;
};

}; // namespace

#endif // LIGHTMAPFILTER_H

