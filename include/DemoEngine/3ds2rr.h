// --------------------------------------------------------------------------
// DemoEngine
// Imports .3ds model into RRVisionApp
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "DemoEngine.h"
#include "RRRealtimeRadiosity.h"
#include "Model_3DS.h"

// new channels supported by our RRObjects
enum
{
	CHANNEL_SURFACE_DIF_TEX          = rr::RRMesh::INDEXED_BY_SURFACE+1, //! Texture*
	CHANNEL_TRIANGLE_VERTICES_DIF_UV = rr::RRMesh::INDEXED_BY_TRIANGLE+5, //! RRVec2[3]
};

// imports all 3d objects from model into RRVisionApp
void RR_API new_3ds_importer(Model_3DS* model,rr::RRVisionApp* app, float stitchDistance);

#endif
