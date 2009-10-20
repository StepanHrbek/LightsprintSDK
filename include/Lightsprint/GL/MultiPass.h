//---------------------------------------------------------------------------
//! \file MultiPass.h
//! \brief LightsprintGL | splits complex rendering setup into simpler passes
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
//---------------------------------------------------------------------------

#ifndef MULTIPASS_H
#define MULTIPASS_H

#include "UberProgramSetup.h"
#include "RendererOfRRObject.h"
#include "PreserveState.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// MultiPass
//
// Splits complex rendering setup (possibly too big for 1 pass) into several simpler setups doable per pass.
// Inputs: lights, uberProgramSetup and stuff for setting shader
// Outputs: each getNextPass() sets shader and returns uberProgramSetup, renderedChannels
// We schedule one light per pass, but it's possible to reimplement MultiPass and shader to render multiple lights per pass.

class RR_GL_API MultiPass
{
public:

	//! MultiPass takes your UberProgramSetup and returns its modified version for individual passes/lights,
	//! taking light properties into account.
	//! For render with all features according to light properties,
	//! set at least SHADOW_MAPS,LIGHT_DIRECT,LIGHT_DIRECT_COLOR,LIGHT_DIRECT_MAP,LIGHT_DIRECT_ATT_SPOT.
	//! If you clear any one of them, it will stay cleared for all lights.
	MultiPass(const RealtimeLights* lights, UberProgramSetup mainUberProgramSetup, UberProgram* uberProgram, const rr::RRVec4* brightness, float gamma, float clipPlaneY);

	//! Returns true and all outXxx are set, do render.
	//! Or returns false and outXxx stay unchanged, rendering is done.
	Program* getNextPass(UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, RealtimeLight*& outLight);

protected:
	Program* getPass(int lightIndex, UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, RealtimeLight*& outLight);

	// inputs
	const RealtimeLights* lights;
	UberProgramSetup mainUberProgramSetup;
	UberProgram* uberProgram;
	const rr::RRVec4* brightness;
	float gamma;
	float clipPlaneY;

	// intermediates
	unsigned numLights;
	int separatedZPass; ///< Z pass is added for blended objects, to avoid blending multiple fragments in undefined order.
	int separatedAmbientPass; ///< Ambient pass is separated if there are no lights to piggyback on.
	int lightIndex;
	PreserveBlend p1;
	PreserveBlendFunc p2;
};

}; // namespace

#endif
