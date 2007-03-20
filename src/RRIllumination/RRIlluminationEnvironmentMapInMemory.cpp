// --------------------------------------------------------------------------
// OpenGL implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#include "RRIlluminationEnvironmentMapInMemory.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMapInMemory

RRIlluminationEnvironmentMapInMemory::RRIlluminationEnvironmentMapInMemory()
{
	data = NULL;
	size = 0;
	data8 = NULL;
	size8 = 0;
}


void RRIlluminationEnvironmentMapInMemory::setValues(unsigned asize, RRColorRGBF* irradiance)
{
	delete[] data;
	size = asize;
	data = new RRColorRGBF[size*size*6];
	memcpy(data,irradiance,size*size*6*sizeof(RRColorRGBF));
}

RRColorRGBF RRIlluminationEnvironmentMapInMemory::getValue(const RRVec3& direction) const
{
	RR_ASSERT(size);
	// find major axis
	RRVec3 d = RRVec3(fabs(direction[0]),fabs(direction[1]),fabs(direction[2]));
	unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 );
	// find side
	unsigned side = 2*axis + ((d[axis]<0)?1:0);
	// find xy
	d = direction / direction[axis];
	RRVec2 xy = RRVec2(d[axis?0:1],d[(axis<2)?2:1]); // -1..1 range
//!!! pro ruzny strany mozna prohodit x<->y nebo negovat
	xy = (xy+RRVec2(1,1))*(0.5f*size); // 0..size range
	unsigned x = (unsigned) CLAMPED((int)xy[0],0,(int)size-1);
	unsigned y = (unsigned) CLAMPED((int)xy[1],0,(int)size-1);
	// read texel
	RR_ASSERT(x<size);
	RR_ASSERT(y<size);
	unsigned ofs = size*size*side+size*y+x;
	if(ofs>=size*size*6)
	{
		RR_ASSERT(0);
		return RRColorRGBF(0);
	}
	return data[size*size*side+size*y+x];
}

void RRIlluminationEnvironmentMapInMemory::bindTexture()
{
	RR_ASSERT(0);
}

RRIlluminationEnvironmentMapInMemory::~RRIlluminationEnvironmentMapInMemory()
{
	delete[] data;
	delete[] data8;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRGPUOpenGL


} // namespace
