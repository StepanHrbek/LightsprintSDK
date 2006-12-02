// --------------------------------------------------------------------------
// DemoEngine
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRRealtimeRadiosity.h"
#include "Model_3DS.h"

// new channels supported by our RRObjects
enum
{
	CHANNEL_SURFACE_DIF_TEX          = rr::RRMesh::INDEXED_BY_SURFACE+1, //! Texture*
	CHANNEL_TRIANGLE_VERTICES_DIF_UV = rr::RRMesh::INDEXED_BY_TRIANGLE+5, //! RRVec2[3]
	CHANNEL_TRIANGLE_OBJECT_ILLUMINATION = rr::RRMesh::INDEXED_BY_TRIANGLE+6, //! RRObjectIllumination*
};

// imports all 3d objects from model into RRRealtimeRadiosity
void DE_API provideObjectsFrom3dsToRR(Model_3DS* model,rr::RRRealtimeRadiosity* app, const rr::RRScene::SmoothingParameters* smoothing);

// if you import 3d objects to RRRealtimeRadiosity with provideObjectsFrom3dsToRR(),
// unimport them with deleteObjectsFromRR()
void DE_API deleteObjectsFromRR(rr::RRRealtimeRadiosity* app);

#endif
