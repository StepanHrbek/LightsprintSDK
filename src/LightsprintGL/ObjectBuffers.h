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

// USE_VBO copies vertex data to VRAM, makes rendering faster.
// Nvidia driver with threaded optimization enabled or auto is known to render rubbish when mixing VBOs and vertex arrays,
// http://www.gamedev.net/community/forums/topic.asp?topic_id=506753
// so it's good idea to use VBO everywhere or nowhere. We do it.
#define USE_VBO

// SMALL_ARRAYS stores vertex data in 8 small arrays rather than 2 big ones.
// Big arrays allocates smaller number of VBOs, it could make init/shutdown faster,
// so we prefer it with VBO. Small arrays are faster without VBO.
//#define SMALL_ARRAYS

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
	//!  False = generates triangle list, numVertices == 3*numTriangles.
	//!  True = generates indexed triangle list, numVertices <= 3*numTriangles, order specified by preimport vertex numbers
	//! \param containsNonBlended
	//!  Set only when returning non-NULL result, otherwise unchanged.
	//! \param containsBlended
	//!  Set only when returning non-NULL result, otherwise unchanged.
	static ObjectBuffers* create(const rr::RRObject* object, bool indexed, bool& containsNonBlended, bool& containsBlended);
	~ObjectBuffers();
	void render(RendererOfRRObject::Params& params, unsigned lightIndirectVersion);
private:
	ObjectBuffers();
	void init(const rr::RRObject* object, bool indexed); // throws std::bad_alloc
	unsigned numVertices;

	// arrays
#ifdef SMALL_ARRAYS
	rr::RRVec3* aposition;
	rr::RRVec3* anormal;
	rr::RRVec2* atexcoordDiffuse;
	rr::RRVec2* atexcoordEmissive;
	rr::RRVec2* atexcoordTransparency;
	rr::RRVec2* atexcoordAmbient; // could be unique for each vertex (with default unwrap)
	rr::RRVec2* atexcoordForced2D; // is unique for each vertex. used only if !indices. filled at render() time. (all other buffers are filled at constructor)
#else
	struct VertexData
	{
		rr::RRVec3 position;
		rr::RRVec3 normal;
		rr::RRVec2 texcoordDiffuse;
		rr::RRVec2 texcoordEmissive;
		rr::RRVec2 texcoordTransparency;
		rr::RRVec2 texcoordAmbient; // could be unique for each vertex (with default unwrap)
		rr::RRVec2 texcoordForced2D; // is unique for each vertex. used only if !indices. filled at render() time. (all other buffers are filled at constructor)
	};
	VertexData* avertex;
#endif
	rr::RRBuffer* alightIndirectVcolor; // used only if !indices. filled at render() time.
	unsigned previousLightIndirectVersion; // version of lightIndirect data we have in VRAM
	rr::RRBuffer* previousLightIndirectBuffer; // layer we copied to VBO_lightIndir last time
	unsigned numIndices;
	unsigned* indices;

	// VBOs
	enum VBOIndex
	{
#ifdef SMALL_ARRAYS
		VBO_position,
		VBO_normal,
		VBO_texcoordDiffuse,
		VBO_texcoordEmissive,
		VBO_texcoordTransparency,
		VBO_texcoordAmbient,
		VBO_texcoordForced2D,
#else
		VBO_vertex,
#endif
		VBO_lightIndirectVcolor,
		VBO_lightIndirectVcolor2, // used when blending 2 vbufs together
		VBO_COUNT
	};
	unsigned VBO[VBO_COUNT];

	// temp 1x1 textures
	std::vector<rr::RRBuffer*> tempTextures;

	struct FaceGroup
	{
		unsigned firstIndex;
		unsigned numIndices;
		rr::RRMaterial material;
		bool needsBlend()
		{
			return material.specularTransmittance.color!=rr::RRVec3(0) && !material.specularTransmittanceKeyed;
		}
	};
	std::vector<FaceGroup> faceGroups;

	bool containsNonBlended;
	bool containsBlended;
};

}; // namespace

#endif
