// --------------------------------------------------------------------------
// Workarounds for driver and GPU quirks.
// Copyright (C) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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
	static bool needsUnfilteredShadowmaps();
	static bool needsIncreasedBias(rr::RRLight& light);
	static bool needsDDI4x4();
	static unsigned needsReducedQualityPenumbra(unsigned SHADOW_MAPS);
};

}; // namespace

#endif
