// --------------------------------------------------------------------------
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Model_3DS.h"

// Custom channels supported by our RRObjects.
// (they allocate channel numbers not used by Lightsprint engine)
enum
{
	CHANNEL_SURFACE_DIF_TEX              = rr::RRMesh::INDEXED_BY_SURFACE+1,  //! channel contains Texture* for each surface
	CHANNEL_TRIANGLE_VERTICES_DIF_UV     = rr::RRMesh::INDEXED_BY_TRIANGLE+5, //! channel contains RRVec2[3] for each triangle
	CHANNEL_TRIANGLE_OBJECT_ILLUMINATION = rr::RRMesh::INDEXED_BY_TRIANGLE+6, //! channel contains RRObjectIllumination* for each triangle
};

//! Imports all 3d objects from model into RRRealtimeRadiosity.
void insert3dsToRR(de::Model_3DS* model,rr::RRRealtimeRadiosity* app, const rr::RRScene::SmoothingParameters* smoothing);

//! Deletes objects previously inserted by insert3dsToRR().
void delete3dsFromRR(rr::RRRealtimeRadiosity* app);

#endif
