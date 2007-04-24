// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include "Lightsprint/RRObject.h"
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
	//!  True = generates indexed triangle list, numVertices <= 3*numTriangles, order specified by preimport vertex numbers
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
	rr::RRVec2* atexcoordEmissive;
	rr::RRVec2* atexcoordAmbient; // could be unique for each vertex (with default unwrap)
	rr::RRVec2* atexcoordForced2D; // is unique for each vertex. used only if !indices. filled at render() time. (all other buffers are filled at constructor)
	rr::RRColor* alightIndirectVcolor; // used only if !indices. filled at render() time.
	unsigned numIndices;
	unsigned* indices;
	struct FaceGroup
	{
		unsigned firstIndex;
		unsigned numIndices;
		rr::RRVec3 diffuseColor;
		de::Texture* diffuseTexture;
		de::Texture* emissiveTexture;
	};
	std::vector<FaceGroup> faceGroups;
};

}; // namespace
