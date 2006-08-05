#ifndef RRILLUMPRECALCULATED_H
#define RRILLUMPRECALCULATED_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumPrecalculated.h
//! \brief RRIllumPrecalculated - library for accessing precalculated illumination
//! \version 2006.8
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMath.h"

/*
#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
			#pragma comment(lib,"RRIllumPrecalculated_s.lib")
		#else
			#pragma comment(lib,"RRIllumPrecalculated_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_PRECALCULATED
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#pragma comment(lib,"RRIllumPrecalculated.lib")
#	endif
#	endif
#endif
*/

#include "RRIllumPrecalculatedSurface.h"
//!!!#include "RRIllumPrecalculatedSpace.h"

#endif
