// --------------------------------------------------------------------------
// Imports Collada model into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RROBJECTCOLLADA_H
#define RROBJECTCOLLADA_H

#include "Lightsprint/RRRealtimeRadiosity.h"


//////////////////////////////////////////////////////////////////////////////
//
// ColladaToRealtimeRadiosity

class ColladaToRealtimeRadiosity
{
public:

	//! Imports FCollada scene contents to RRRealtimeRadiosity solver.
	ColladaToRealtimeRadiosity(class FCDocument* document,rr::RRRealtimeRadiosity* solver,const rr::RRScene::SmoothingParameters* smoothing);

	//! Removes FCollada scene contents from RRRealtimeRadiosity solver.
	~ColladaToRealtimeRadiosity();

private:
	class ColladaToRealtimeRadiosityImpl* impl;
};

#endif
