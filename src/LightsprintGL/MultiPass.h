//---------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Splits complex rendering setup into simpler passes.
//---------------------------------------------------------------------------

#ifndef MULTIPASS_H
#define MULTIPASS_H

#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// MultiPass
//
// Splits complex rendering setup (possibly too big for 1 pass) into several simpler setups doable per pass.
// Inputs: lights, uberProgramSetup and stuff for setting shader
// Outputs: each getNextPass() sets shader and returns uberProgramSetup
// It schedules one light per pass, but it's possible to reimplement MultiPass and shader to render multiple lights per pass.

class RR_GL_API MultiPass
{
public:

	//! MultiPass takes your UberProgramSetup and returns its modified version for individual passes/lights,
	//! taking light properties into account.
	//! For render with all features according to light properties,
	//! set at least SHADOW_MAPS,LIGHT_DIRECT,LIGHT_DIRECT_COLOR,LIGHT_DIRECT_MAP,LIGHT_DIRECT_ATT_SPOT.
	//! If you clear any one of them, it will stay cleared for all lights.
	MultiPass(const rr::RRCamera& camera, const RealtimeLights* lights, const RealtimeLight* renderingFromThisLight, UberProgramSetup mainUberProgramSetup, UberProgram* uberProgram,
		float lightDirectMultiplier, const ClipPlanes* clipPlanes, bool srgbCorrect, const rr::RRVec4* brightness, float gamma);

	//! Returns true and all outXxx are set, do render.
	//! Or returns false and outXxx stay unchanged, rendering is done.
	//! Note that functions sets OpenGL blending and masking states (glBlendFunc, GL_BLEND, glDepthMask, glColorMask) for given pass,
	//! and does not restore them at the end of rendering, caller is responsible.
	Program* getNextPass(UberProgramSetup& outUberProgramSetup, RealtimeLight*& outLight);

protected:
	Program* getPass(int lightIndex, UberProgramSetup& outUberProgramSetup, RealtimeLight*& outLight);

	// inputs
	const rr::RRCamera& camera;
	const RealtimeLights* lights;
	UberProgramSetup mainUberProgramSetup;
	UberProgram* uberProgram;
	float lightDirectMultiplier;
	const rr::RRVec4* brightness;
	float gamma;
	const ClipPlanes* clipPlanes;

	// intermediates
	GLboolean depthMask;
	GLboolean colorMask;
	int separatedZPass; ///< Z pass is added for blended objects, to avoid blending multiple fragments in undefined order.
	int separatedMultiplyPass; ///< Multiply pass (without Z write) is for blended objects (with MATERIAL_TRANSPARENCY_TO_RGB).
	int separatedAmbiEmiPass; ///< Ambient/emissive pass is separated if there are no lights to piggyback on.
	int lightIndex; // getNextPass() may increase it by more than one in presence of disabled light.
	int colorPassIndex; // getNextPass increases it always by one. Starts at -1 for Z-only pass, 0 otherwise.
	PreserveBlend p1;
	PreserveBlendFunc p2;
};

}; // namespace

#endif
