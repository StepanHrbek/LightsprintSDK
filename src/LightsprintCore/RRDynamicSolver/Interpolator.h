// --------------------------------------------------------------------------
// Parallel interpolator.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <vector>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRLight.h" // RRScaler

namespace rr
{

class Interpolator
{
public:
	Interpolator();

	// Learning process
	// To be called in this order:
	// for each destination
	// {
	//   learnDestinationBegin();
	//   for each source learnSource();
	//   learDestinationEnd();
	// }
	void learnDestinationBegin();
	void learnSource(unsigned offset, float contribution); // what to interpolate, contribution in <0,1>
	void learnDestinationEnd(unsigned offset1); // where to write result

	unsigned getDestinationSize() const;

	// High quality interpolation in physical space.
	void interpolate(const RRVec3* src, RRBuffer* dst, const RRScaler* scaler) const;

private:
	typedef unsigned Ofs;
	struct Header
	{
		Ofs srcContributorsBegin;
		Ofs srcContributorsEnd;
		Ofs dstOffset1;
	};
	struct Contributor
	{
		float srcContributionHdr;
		Ofs srcOffset;
	};
	Ofs srcBegin;
	std::vector<Header> headers;
	std::vector<Contributor> contributors;
	unsigned sourceSize;
	unsigned destinationSize;
};

}; // namespace

#endif

