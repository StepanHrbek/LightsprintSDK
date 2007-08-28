// --------------------------------------------------------------------------
// System memory implementation of interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONENVIRONMENTMAPINMEMORY_H
#define RRILLUMINATIONENVIRONMENTMAPINMEMORY_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/Texture.h"

namespace rr
{

	class RRIlluminationEnvironmentMapInMemory : public RRIlluminationEnvironmentMap
	{
	public:
		RRIlluminationEnvironmentMapInMemory(const char* filename, const char* cubeSideName[6], bool flipV = false, bool flipH = false);
		RRIlluminationEnvironmentMapInMemory(unsigned width);
		virtual void setValues(unsigned size, const RRColorRGBF* irradiance);
		virtual RRColorRGBF getValue(const RRVec3& direction) const;
		virtual void bindTexture() const;
		virtual bool save(const char* filenameMask, const char* cubeSideName[6]);
		virtual ~RRIlluminationEnvironmentMapInMemory();
	private:
		friend class RRIlluminationEnvironmentMap;
		rr_gl::Texture* texture;
	};

} // namespace

#endif
