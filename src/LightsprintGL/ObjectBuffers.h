// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef OBJECTBUFFERS_H
#define OBJECTBUFFERS_H

#include <vector>
#include "Lightsprint/RRObject.h"
#include "Lightsprint/GL/Texture.h"

namespace rr_gl
{

// Specifies size of texture used by DDI.
// 256 ->  64k triangles per DDI pass, 2k or 1k texture
// 512 -> 256k triangles per DDI pass, 4k or 2k texture
#define DDI_TRIANGLES_X      256 // number of triangles in DDI big map row
#define DDI_TRIANGLES_MAX_Y  256 // max number of triangles in DDI big map column


//////////////////////////////////////////////////////////////////////////////
//
// MeshVBOs - RRMesh data stored in indexed and !indexed VBOs

class MeshVBOs : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	MeshVBOs();
	~MeshVBOs();
protected:
	//! Fills buffer when requested for first time.
	//! Updates it if (mesh pointer or numTriangles or numVertices changes) or (indexed and mesh is RRMeshArrays).
	class MeshArraysVBOs* getMeshArraysVBOs(const rr::RRMesh* mesh, bool indexed);
private:
	const void*           createdFromMesh[2];
	unsigned              createdFromNumTriangles[2];
	unsigned              createdFromNumVertices[2];
	class MeshArraysVBOs* meshArraysVBOs[2];
};

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfMesh - hides indexed/!indexed complexity of MeshArraysVBOs

class RendererOfMesh : public MeshVBOs
{
public:
	//! Renders mesh VBOs.
	//! Must not be called inside display list (may create VBOs, textures).
	void render(RendererOfRRObject& params);
};

}; // namespace

#endif
