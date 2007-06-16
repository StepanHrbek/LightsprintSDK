#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif

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
	RR_ASSERT(srcBegin == contributors.size());
}

void Interpolator::learnSource(unsigned offset, float contribution)
{
	Contributor c;
	c.srcOffset = offset;
	c.srcContributionHdr = contribution;
#ifdef SUPPORT_LDR
	c.srcContributionLdr = (u16)(contribution*65536);
#endif
	RR_ASSERT(_finite(contribution));
	contributors.push_back(c);
}

void Interpolator::learnDestinationEnd(unsigned offset1, unsigned offset2, unsigned offset3)
{
	Header h;
	h.srcContributorsBegin = srcBegin;
	h.srcContributorsEnd = (Ofs)contributors.size();
	RR_ASSERT(h.srcContributorsEnd == contributors.size());
	RR_ASSERT(h.srcContributorsEnd>h.srcContributorsBegin); // radius too small -> no neighbour texels put into interoplation
	h.dstOffset1 = offset1;
#ifdef THREE_DESTINATIONS
	h.dstOffset2 = offset2;
	h.dstOffset3 = offset3;
#endif
	headers.push_back(h);
	destinationSize = MAX(offset1+1,destinationSize);
#ifdef THREE_DESTINATIONS
	destinationSize = MAX(offset2+1,destinationSize);
	destinationSize = MAX(offset3+1,destinationSize);
#endif
}

unsigned Interpolator::getDestinationSize() const
{
	return destinationSize;
}

void Interpolator::interpolate(const RRColor* src, RRColor* dst, const RRScaler* scaler) const
{
	#pragma omp parallel for schedule(static) // fastest: static, dynamic, static,1
	for(int i=0;i<(int)headers.size();i++)
	{
		RRColor sum = RRColor(0);
		RR_ASSERT(headers[i].srcContributorsBegin<headers[i].srcContributorsEnd);
		for(unsigned j=headers[i].srcContributorsBegin;j<headers[i].srcContributorsEnd;j++)
		{
			RR_ASSERT(_finite(contributors[j].srcContributionHdr));
			RR_ASSERT(contributors[j].srcContributionHdr>0);
			sum += src[contributors[j].srcOffset] * contributors[j].srcContributionHdr;
		}
		if(scaler) scaler->getCustomScale(sum);
#ifdef THREE_DESTINATIONS
		dst[headers[i].dstOffset3] = dst[headers[i].dstOffset2] =
#endif
			dst[headers[i].dstOffset1] = sum;
	}
}

#ifdef SUPPORT_LDR
// pozor, asi nekde preteka, stredy stran cubemapy byly cerny
void Interpolator::interpolate(const RRColorRGBA8* src, RRColorRGBA8* dst, void* unused) const
{
	#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)headers.size();i++)
	{
		unsigned sum[3] = {0,0,0};
		RR_ASSERT(headers[i].srcContributorsBegin<headers[i].srcContributorsEnd);
		for(unsigned j=headers[i].srcContributorsBegin;j<headers[i].srcContributorsEnd;j++)
		{
			unsigned color = src[contributors[j].srcOffset].color;
			RR_ASSERT(contributors[j].srcContributionLdr);
			sum[0] += u8(color) * contributors[j].srcContributionLdr;
			sum[1] += u8(color>>8) * contributors[j].srcContributionLdr;
			sum[2] += u8(color>>16) * contributors[j].srcContributionLdr;
		}
#ifdef THREE_DESTINATIONS
		dst[headers[i].dstOffset3] = dst[headers[i].dstOffset2] =
#endif
			dst[headers[i].dstOffset1].color = (sum[0]>>16) + ((sum[1]>>16)<<8) + (sum[2]&0xff0000);
	}
}
#endif

} // namespace
