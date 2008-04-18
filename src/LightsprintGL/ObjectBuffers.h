// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef OBJECTBUFFERS_H
#define OBJECTBUFFERS_H

#include <vector>
#include "Lightsprint/RRObject.h"
#include "Lightsprint/GL/Texture.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// ObjectBuffers - RRObject data stored in buffers for faster rendering

class ObjectBuffers
{
public:
	//! \param indexed
	//!  False = generates triangle list, numVertices == 3*numTriangles.
	//!  True = generates indexed triangle list, numVertices <= 3*numTriangles, order specified by preimport vertex numbers
	static ObjectBuffers* create(const rr::RRObject* object, bool indexed);
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
	rr::RRVec2* atexcoordAmbient; // could be unique for each vertex (with default unwrap)
	rr::RRVec2* atexcoordForced2D; // is unique for each vertex. used only if !indices. filled at render() time. (all other buffers are filled at constructor)
	rr::RRBuffer* alightIndirectVcolor; // used only if !indices. filled at render() time.
	unsigned numIndices;
	unsigned* indices;

	// VBOs
	unsigned vertexVBO;
	unsigned normalVBO;
	unsigned texcoordDiffuseVBO;
	unsigned texcoordEmissiveVBO;
	unsigned texcoordAmbientVBO;
	//unsigned texcoordForced2DVBO;
	//unsigned lightIndirectVcolorVBO;

	struct FaceGroup
	{
		unsigned firstIndex;
		unsigned numIndices:30;
		unsigned renderFront:1;
		unsigned renderBack:1;
		rr::RRBuffer* diffuseTexture;
		rr::RRBuffer* emissiveTexture;
		rr::RRVec4 diffuseColor; // last component is alpha, 0=transparent
		rr::RRVec3 emissiveColor;
	};
	std::vector<FaceGroup> faceGroups;
	// version of data in alightIndirectVcolor (we don't want to update data when it's not necessary)
	unsigned lightIndirectVcolorVersion;
	unsigned lightIndirectVcolorFirst;
	unsigned lightIndirectVcolorLastPlus1;
};

}; // namespace

#endif
