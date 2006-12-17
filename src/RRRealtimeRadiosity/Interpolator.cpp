#include <cassert>
#include <cfloat>
//#ifdef _OPENMP
//#include <omp.h>
//#endif
#include "Interpolator.h"

#define MAX(a,b) (((a)>(b))?(a):(b))
typedef unsigned char u8;

namespace rr
{

Interpolator::Interpolator()
{
	destinationSize = 0;
}

void Interpolator::learnDestinationBegin()
{
	srcBegin = (Ofs)contributors.size();
	assert(srcBegin == contributors.size());
}

void Interpolator::learnSource(unsigned offset, float contribution)
{
	Contributor c;
	c.srcOffset = offset;
	c.srcContributionHdr = contribution;
	c.srcContributionLdr = (u16)(contribution*65536);
	assert(_finite(contribution));
	contributors.push_back(c);
}

void Interpolator::learnDestinationEnd(unsigned offset1, unsigned offset2, unsigned offset3)
{
	Header h;
	h.srcContributorsBegin = srcBegin;
	h.srcContributorsEnd = (Ofs)contributors.size();
	assert(h.srcContributorsEnd == contributors.size());
	assert(h.srcContributorsEnd>h.srcContributorsBegin); // radius too small -> no neighbour texels put into interoplation
	h.dstOffset1 = offset1;
	h.dstOffset2 = offset2;
	h.dstOffset3 = offset3;
	headers.push_back(h);
	destinationSize = MAX(offset1+1,destinationSize);
	destinationSize = MAX(offset2+1,destinationSize);
	destinationSize = MAX(offset3+1,destinationSize);
}

unsigned Interpolator::getDestinationSize() const
{
	return destinationSize;
}

void Interpolator::interpolateHdr(const RRColor* src, RRColor* dst, const RRScaler* scaler) const
{
	//#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)headers.size();i++)
	{
		RRColor sum = RRColor(0);
		for(unsigned j=headers[i].srcContributorsBegin;j<headers[i].srcContributorsEnd;j++)
		{
			assert(_finite(contributors[j].srcContributionHdr));
			sum += src[contributors[j].srcOffset] * contributors[j].srcContributionHdr;
		}
		if(scaler) scaler->getCustomScale(sum);
		dst[headers[i].dstOffset3] = dst[headers[i].dstOffset2] = dst[headers[i].dstOffset1] = sum;
	}
}

void Interpolator::interpolateLdr(const RRColorRGBA8* src, RRColorRGBA8* dst) const
{
	//#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)headers.size();i++)
	{
		unsigned sum[3] = {0,0,0};
		for(unsigned j=headers[i].srcContributorsBegin;j<headers[i].srcContributorsEnd;j++)
		{
			unsigned color = src[contributors[j].srcOffset].color;
			sum[0] += u8(color) * contributors[j].srcContributionLdr;
			sum[1] += u8(color>>8) * contributors[j].srcContributionLdr;
			sum[2] += u8(color>>16) * contributors[j].srcContributionLdr;
		}
		RRColorRGBA8 result;
		result.color = (sum[0]>>16) + ((sum[1]>>16)<<8) + (sum[2]&0xff0000);
		dst[headers[i].dstOffset3] = dst[headers[i].dstOffset2] = dst[headers[i].dstOffset1] = result;
	}
}

} // namespace
