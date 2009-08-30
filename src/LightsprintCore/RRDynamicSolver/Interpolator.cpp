// --------------------------------------------------------------------------
// Parallel interpolator.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "Interpolator.h"
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRLight.h"

namespace rr
{

Interpolator::Interpolator()
{
	sourceSize = 0;
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
	sourceSize = RR_MAX(sourceSize,offset+1);
	c.srcContributionHdr = contribution;
	RR_ASSERT(_finite(contribution));
	contributors.push_back(c);
}

void Interpolator::learnDestinationEnd(unsigned offset1)
{
	Header h;
	h.srcContributorsBegin = srcBegin;
	h.srcContributorsEnd = (Ofs)contributors.size();
	RR_ASSERT(h.srcContributorsEnd == contributors.size());
	RR_ASSERT(h.srcContributorsEnd>h.srcContributorsBegin); // radius too small -> no neighbour texels put into interoplation
	h.dstOffset1 = offset1;
	headers.push_back(h);
	destinationSize = RR_MAX(offset1+1,destinationSize);
}

unsigned Interpolator::getDestinationSize() const
{
	return destinationSize;
}

// caller must ensure that dst is of proper size
void Interpolator::interpolate(const RRVec3* src, RRBuffer* dst, const RRScaler* scaler) const
{
	RR_ASSERT(dst);
	if (!dst->getScaled()) scaler = NULL;
	unsigned char* locked = dst->lock(BL_DISCARD_AND_WRITE);
	RRBufferFormat format = dst->getFormat();
#pragma omp parallel for schedule(static) // fastest: static, dynamic, static,1
	for (int i=0;i<(int)headers.size();i++)
	{
		RRVec4 sum = RRVec4(0);
		RR_ASSERT(headers[i].srcContributorsBegin<headers[i].srcContributorsEnd);
		for (unsigned j=headers[i].srcContributorsBegin;j<headers[i].srcContributorsEnd;j++)
		{
			RR_ASSERT(_finite(contributors[j].srcContributionHdr));
			RR_ASSERT(contributors[j].srcContributionHdr>0);
			sum.RRVec3::operator+=(src[contributors[j].srcOffset] * contributors[j].srcContributionHdr);
		}
		if (scaler) scaler->getCustomScale(sum);
		if (locked)
		{
			switch(format)
			{
				case BF_RGBF:
					((RRVec3*)locked)[headers[i].dstOffset1] = sum;
					break;
				case BF_RGB:
					locked[headers[i].dstOffset1*3  ] = RR_FLOAT2BYTE(sum[0]);
					locked[headers[i].dstOffset1*3+1] = RR_FLOAT2BYTE(sum[1]);
					locked[headers[i].dstOffset1*3+2] = RR_FLOAT2BYTE(sum[2]);
					break;
				case BF_RGBAF:
					((RRVec4*)locked)[headers[i].dstOffset1] = sum;
					break;
				case BF_RGBA:
					locked[headers[i].dstOffset1*4  ] = RR_FLOAT2BYTE(sum[0]);
					locked[headers[i].dstOffset1*4+1] = RR_FLOAT2BYTE(sum[1]);
					locked[headers[i].dstOffset1*4+2] = RR_FLOAT2BYTE(sum[2]);
					locked[headers[i].dstOffset1*4+3] = 0;
					break;
				default:
					RRReporter::report(WARN,"Unexpected buffer format.\n");
					break;
			}
		}
		else
		{
			dst->setElement(headers[i].dstOffset1,sum);
		}
	}
	if (locked) dst->unlock();
}

} // namespace

