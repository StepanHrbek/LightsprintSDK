//
// 3ds to rrobject loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRIllumCalculator.h"
#include "Model_3DS.h"

// new channels supported by our RRObjects
enum
{
	CHANNEL_SURFACE_DIF_TEX          = rr::RRMesh::INDEXED_BY_SURFACE+1, //! Texture*
	CHANNEL_TRIANGLE_VERTICES_DIF_UV = rr::RRMesh::INDEXED_BY_TRIANGLE+5, //! RRVec2[3]
};

// you may load object from 3ds
void new_3ds_importer(Model_3DS* model,rr::RRVisionApp* app, float stitchDistance);

#endif
