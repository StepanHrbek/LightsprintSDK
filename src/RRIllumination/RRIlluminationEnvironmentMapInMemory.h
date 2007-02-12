// --------------------------------------------------------------------------
// System memory implementation of environment map interface rr::RRIlluminationEnvironmentMap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONENVIRONMENTMAPINMEMORY_H
#define RRILLUMINATIONENVIRONMENTMAPINMEMORY_H

#include "RRIllumination.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in environment map in system memory.
	//
	//! Thread safe: no,
	//!    setValues could allocate memory.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationEnvironmentMapInMemory : public rr::RRIlluminationEnvironmentMap
	{
	public:
		RRIlluminationEnvironmentMapInMemory();
		virtual void setValues(unsigned size, rr::RRColorRGBF* irradiance);
		virtual RRColorRGBF getValue(const RRVec3& direction) const;
		virtual void bindTexture();
		virtual ~RRIlluminationEnvironmentMapInMemory();
	private:
		unsigned size8;
		RRColorRGBA8* data8;
		unsigned size;
		RRColorRGBF* data;
	};

} // namespace

#endif
