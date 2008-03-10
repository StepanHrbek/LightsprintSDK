// --------------------------------------------------------------------------
// Misc.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>

#include "../LicGen.h"
#include "rrcore.h"
#include "../RRStaticSolver/RRStaticSolver.h"

namespace rr
{


RRLicense::LicenseStatus RRLicense::loadLicense(const char* filename)
{
	return RRLicense::VALID;
}

} // namespace
