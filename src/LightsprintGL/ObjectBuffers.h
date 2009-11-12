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
// ObjectBuffers - RRObject data stored in buffers for faster rendering

class ObjectBuffers
{
public:
	//! \param indexed
	//!  False = generates triangle list, numVertices == 3*mesh->getNumTriangles().
	//!  True = generates indexed triangle list, numVertices == mesh->getNumVertices(), order specified by postimport vertex numbers
	//! \param containsNonBlended
	//!  Set to 0 or 1, 1=object contains materials that don't need blending.
	//!  Touched only when returning non-NULL result, otherwise unchanged.
	//! \param containsBlended
	//!  Set to 0 or 1, 1=object contains materials that need blending.
	//!  Touched only when returning non-NULL result, otherwise unchanged.
	//! \param unwrapSplitsVertices
	//!  Must be inited to 0, set to 1 if unwrap assigns different uvs to single vertex.
	//!  Touched only when returning non-NULL result, otherwise unchanged.
	static ObjectBuffers* create(const rr::RRObject* object, bool indexed, bool& containsNonBlended, bool& containsBlended, bool& unwrapSplitsVertices);
	~ObjectBuffers();
	void render(RendererOfRRObject::Params& params, unsigned lightIndirectVersion);
private:
	ObjectBuffers();
	void init(const rr::RRObject* object, bool indexed); // throws std::bad_alloc
	unsigned numVertices;

	// arrays
	rr::RRVec3* aposition;
	rr::RRVec3* anormal;
	rr::RRVec2* atexcoordDiffuse;
	rr::RRVec2* atexcoordEmissive;
	rr::RRVec2* atexcoordTransparency;
	rr::RRVec2* atexcoordAmbient; // could be unique for each vertex (with default unwrap)
	rr::RRVec2* atexcoordForced2D; // is unique for each vertex. used only if !indices. filled at render() time. (all other buffers are filled at constructor)
	rr::RRBuffer* alightIndirectVcolor; // used only if !indices. filled at render() time.
	unsigned previousLightIndirectVersion; // version of lightIndirect data we have in VRAM
	rr::RRBuffer* previousLightIndirectBuffer; // layer we copied to VBO_lightIndir last time
	unsigned numIndicesObj; // 0=!indexed, !0=indexed
	unsigned* indices;

	// VBOs
	enum VBOIndex
	{
		VBO_index, // used only if indexed
		VBO_position,
		VBO_normal,
		VBO_texcoordDiffuse,
		VBO_texcoordEmissive,
		VBO_texcoordTransparency,
		VBO_texcoordAmbient,
		VBO_texcoordForced2D, // used only if !indexed
		VBO_lightIndirectVcolor,
		VBO_lightIndirectVcolor2, // used when blending 2 vbufs together
		VBO_COUNT
	};
	GLuint VBO[VBO_COUNT];
	// optimization -  removes redundant VBOs
	bool texcoordEmissiveIsInDiffuse; // emissive not allocated, use diffuse, is identical
	bool texcoordTransparencyIsInDiffuse; // transparent not allocated, use diffuse, is identical
	VBOIndex fixVBO(VBOIndex index) const; // changes removed VBO index to identical valid index

	// temp 1x1 textures
	std::vector<rr::RRBuffer*> tempTextures;

	struct IndexGroup
	{
		unsigned firstIndex;
		unsigned numIndices;
	};
	struct FaceGroup : public IndexGroup
	{
		rr::RRMaterial material;
		bool needsBlend()
		{
			return material.specularTransmittance.color!=rr::RRVec3(0) && !material.specularTransmittanceKeyed;
		}
	};
	std::vector<FaceGroup> faceGroups;

	bool containsNonBlended;
	bool containsBlended;
	bool unwrapSplitsVertices; // true if indexed and unwrap needs !indexed render because it assigns different uvs to single vertex
};

}; // namespace

#endif
