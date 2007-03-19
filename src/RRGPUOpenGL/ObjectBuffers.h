// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include "Lightsprint/RRVision.h"
#include "Lightsprint/DemoEngine/Texture.h"

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
	//!  True = generates indexed triangle list, numIndices <= 3*numTriangles (merges identical vertices).
	ObjectBuffers(const rr::RRObject* object, bool indexed);
	~ObjectBuffers();
	bool inited();
	void render(RendererOfRRObject::Params& params);
private:
	bool initedOk; // true when constructor had no problems and instance is ready to render
	unsigned numVertices;
	rr::RRVec3* avertex;
	rr::RRVec3* anormal;
	rr::RRVec2* atexcoordDiffuse;
	rr::RRVec2* atexcoordForced2D; // is unique for each vertex
	rr::RRVec2* atexcoordAmbient; // could be unique for each vertex (with default unwrap)
	unsigned numIndices;
	unsigned* indices;
	struct FaceGroup
	{
		unsigned firstIndex;
		unsigned numIndices;
		rr::RRVec3 diffuseColor;
		de::Texture* diffuseTexture;
	};
	std::vector<FaceGroup> faceGroups;
};

}; // namespace
