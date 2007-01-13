// --------------------------------------------------------------------------
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Model_3DS.h"

// new channels supported by our RRObjects
// used only when ambient maps are rendered
enum
{
	CHANNEL_SURFACE_DIF_TEX              = rr::RRMesh::INDEXED_BY_SURFACE+1,  //! Texture*
	CHANNEL_TRIANGLE_VERTICES_DIF_UV     = rr::RRMesh::INDEXED_BY_TRIANGLE+5, //! RRVec2[3]
	CHANNEL_TRIANGLE_OBJECT_ILLUMINATION = rr::RRMesh::INDEXED_BY_TRIANGLE+6, //! RRObjectIllumination*
};

// imports all 3d objects from model into RRRealtimeRadiosity
void provideObjectsFrom3dsToRR(de::Model_3DS* model,rr::RRRealtimeRadiosity* app, const rr::RRScene::SmoothingParameters* smoothing);

// if you import 3d objects to RRRealtimeRadiosity with provideObjectsFrom3dsToRR(),
// unimport them with deleteObjectsFromRR()
void deleteObjectsFromRR(rr::RRRealtimeRadiosity* app);

#endif
