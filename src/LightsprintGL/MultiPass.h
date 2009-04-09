// --------------------------------------------------------------------------
// MultiPass splits complex rendering setup into simpler ones doable per pass.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef MULTIPASS_H
#define MULTIPASS_H

#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/RendererOfRRObject.h"

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

class MultiPass
{
public:
	MultiPass(const RealtimeLights* lights, UberProgramSetup mainUberProgramSetup, UberProgram* uberProgram, const rr::RRVec4* brightness, float gamma);

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

	// intermediates
	unsigned numLights;
	int separatedZPass; //! Z pass is added for blended objects, to avoid blending multiple fragments in undefined order.
	int separatedAmbientPass; //! Ambient pass is separated if there are no lights to piggyback on.
	int lightIndex;
};

}; // namespace

#endif
