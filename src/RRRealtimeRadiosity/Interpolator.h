#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <vector>
#include "RRIllumination.h"
#include "RRVision.h"

namespace rr
{

typedef unsigned short u16;

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
	void learnDestinationEnd(unsigned offset1, unsigned offset2, unsigned offset3); // where to write result

	unsigned getDestinationSize() const;

	// High quality interpolation in physical space.
	void interpolate(const RRColor* src, RRColor* dst, const RRScaler* scaler) const;
	// Fast interpolation in custom space.
	void interpolate(const RRColorRGBA8* src, RRColorRGBA8* dst, void* unused) const;

private:
	typedef unsigned Ofs;
	struct Header
	{
		Ofs srcContributorsBegin;
		Ofs srcContributorsEnd;
		Ofs dstOffset1;
		Ofs dstOffset2;
		Ofs dstOffset3;
	};
	struct Contributor
	{
		float srcContributionHdr;
		Ofs srcOffset;
		u16 srcContributionLdr;
	};
	Ofs srcBegin;
	std::vector<Header> headers;
	std::vector<Contributor> contributors;
	unsigned destinationSize;
};

}; // namespace

#endif
