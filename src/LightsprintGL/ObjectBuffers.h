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
	void render(RendererOfRRObject::Params& params, unsigned solutionVersion);
private:
	ObjectBuffers();
	void init(const rr::RRObject* object, bool indexed); // throws std::bad_alloc
	unsigned numVertices;

	// arrays
	rr::RRVec3* avertex;
	rr::RRVec3* anormal;
	rr::RRVec2* atexcoordDiffuse;
	rr::RRVec2* atexcoordEmissive;
	rr::RRVec2* atexcoordTransparency;
	rr::RRVec2* atexcoordAmbient; // could be unique for each vertex (with default unwrap)
	rr::RRVec2* atexcoordForced2D; // is unique for each vertex. used only if !indices. filled at render() time. (all other buffers are filled at constructor)
	rr::RRBuffer* alightIndirectVcolor; // used only if !indices. filled at render() time.
	unsigned lightIndirectVcolorVersion; // version of data in alightIndirectVcolor (we don't want to update data when it's not necessary)
	unsigned numIndices;
	unsigned* indices;

	// VBOs
	enum VBOIndex
	{
		VBO_vertex,
		VBO_normal,
		VBO_texcoordDiffuse,
		VBO_texcoordEmissive,
		VBO_texcoordTransparency,
		VBO_texcoordAmbient,
		VBO_texcoordForced2D,
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
