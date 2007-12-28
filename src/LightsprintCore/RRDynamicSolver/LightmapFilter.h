#ifndef LIGHTMAPFILTER_H
#define LIGHTMAPFILTER_H

#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

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
