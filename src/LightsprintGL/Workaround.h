// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Workarounds for driver and GPU quirks.
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
