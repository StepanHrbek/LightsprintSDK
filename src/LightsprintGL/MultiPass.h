// --------------------------------------------------------------------------
// MultiPass splits complex rendering setup into simpler ones doable per pass.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
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
// We schedule one light per pass, but it's possible to reimplement here (and in shader) to render multiple lights per pass.

class MultiPass
{
public:
	MultiPass(const RealtimeLights* lights, UberProgramSetup mainUberProgramSetup, UberProgram* uberProgram, const rr::RRVec4* brightness, float gamma, bool honourExpensiveLightingShadowingFlags);

	// Returns true and all outXxx are set, do render,
	// or returns false and outXxx stay unchanged, rendering is done.
	Program* getNextPass(UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, const RealtimeLight*& outLight);

protected:
	Program* getPass(int lightIndex, UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, const RealtimeLight*& outLight);

	// inputs
	const RealtimeLights* lights;
	UberProgramSetup mainUberProgramSetup;
	UberProgram* uberProgram;
	const rr::RRVec4* brightness;
	float gamma;
	bool honourExpensiveLightingShadowingFlags;

	// intermediates
	unsigned numLights;
	int separatedAmbientPass;
	int lightIndex;
};

}; // namespace

#endif
