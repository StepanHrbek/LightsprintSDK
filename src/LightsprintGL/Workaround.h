// --------------------------------------------------------------------------
// Workarounds for driver and GPU quirks.
// Copyright (C) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef WORKAROUND_H
#define WORKAROUND_H

#include "Lightsprint/RRLight.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Workaround

class Workaround
{
public:
	// is workaround needed?
	static bool needsUnfilteredShadowmaps();
	static bool needsOneSampleShadowmaps(const rr::RRLight& light);
	static void needsIncreasedBias(float& slopeBias,float& fixedBias,const rr::RRLight& light);
	static bool needsDDI4x4();
	static bool needsHardPointShadows();
	static unsigned needsReducedQualityPenumbra(unsigned SHADOW_MAPS);
    static bool needsNoLods();

	// is feature supported?
	static bool supportsDepthClamp();
	static bool supportsSRGB();
};

}; // namespace

#endif
