// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
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
// FaceGroupRange

struct FaceGroupRange
{
	unsigned short object;
	unsigned short faceGroupFirst;
	unsigned short faceGroupLast;
	unsigned triangleFirst;
	unsigned triangleLastPlus1;
	FaceGroupRange(unsigned _object, unsigned _faceGroup, unsigned _triangleFirst, unsigned _triangleLastPlus1)
	{
		object = _object;
		faceGroupFirst = _faceGroup;
		faceGroupLast = _faceGroup;
		triangleFirst = _triangleFirst;
		triangleLastPlus1 = _triangleLastPlus1;
		RR_ASSERT(object==_object);
		RR_ASSERT(faceGroupFirst==_faceGroup);
	}
	FaceGroupRange(unsigned _object, unsigned _faceGroupFirst, unsigned _faceGroupLast, unsigned _triangleFirst, unsigned _triangleLastPlus1)
	{
		object = _object;
		faceGroupFirst = _faceGroupFirst;
		faceGroupLast = _faceGroupLast;
		triangleFirst = _triangleFirst;
		triangleLastPlus1 = _triangleLastPlus1;
		RR_ASSERT(object==_object);
		RR_ASSERT(faceGroupFirst==_faceGroupFirst);
		RR_ASSERT(faceGroupLast==_faceGroupLast);
	}
	bool operator <(const FaceGroupRange& a) const; // for sort()
};


//////////////////////////////////////////////////////////////////////////////
//
// MeshArraysVBOs - RRMeshArrays data stored in VBOs for faster rendering

class MeshArraysVBOs : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	MeshArraysVBOs();
	~MeshArraysVBOs();

	//! Updates mesh VBOs.
	//! Must not be called inside display list (may create VBOs).
	//! \param mesh
	//!  VBOs are created/updated for this mesh.
	//! \param indexed
	//!  False = generates triangle list, numVertices == 3*mesh->getNumTriangles().
	//!  True = generates indexed triangle list, numVertices == mesh->getNumVertices(), order specified by postimport vertex numbers
	//! \return
	//!  True if update succeeded or was not necessary. False if update failed.
	bool update(const rr::RRMeshArrays* mesh, bool indexed);

	//! Renders mesh.
	//! Must not be called inside display list (may create textures).
	void render(
		Program* program,
		const rr::RRObject* object,
		const FaceGroupRange* faceGroupRange,
		unsigned numFaceGroupRanges,
		const UberProgramSetup& uberProgramSetup,
		bool renderingFromLight, // true = renders also invisible backfaces (!renderFrom) that block photons (catchFrom)
		rr::RRBuffer* lightIndirectBuffer,
		const rr::RRBuffer* lightDetailMap);

private:
	const void*   createdFromMesh;
	unsigned      createdFromMeshVersion;
	const void*   createdFromLightIndirectBuffer;
	unsigned      createdFromLightIndirectVersion;
	bool          createdIndexed;
	bool          createdOk;
	bool          hasTangents;

	enum VBOIndex
	{
		VBO_index, // used only if indexed
		VBO_position,
		VBO_normal,
		VBO_tangent,
		VBO_bitangent,
		VBO_lightIndirectVcolor,
		VBO_texcoordForced2D,
		VBO_COUNT
	};
	GLuint VBO[VBO_COUNT];
	rr::RRVector<GLuint> texcoordVBO;
};


//////////////////////////////////////////////////////////////////////////////
//
// MeshVBOs - RRMesh data stored in indexed and !indexed VBOs

class MeshVBOs : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	MeshVBOs();
protected:
	//! Fills buffer when requested for first time.
	//! Updates it if (mesh pointer or numTriangles or numVertices changes) or (indexed and mesh is RRMeshArrays).
	class MeshArraysVBOs* getMeshArraysVBOs(const rr::RRMesh* mesh, bool indexed);
private:
	const void*           createdFromMesh[2];
	unsigned              createdFromNumTriangles[2];
	unsigned              createdFromNumVertices[2];
	bool                  updatedOk[2];
	MeshArraysVBOs        meshArraysVBOs[2];
};


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfMesh - hides indexed/!indexed complexity of MeshArraysVBOs

class RendererOfMesh : public MeshVBOs
{
public:
	//! Renders mesh.
	//! Must not be called inside display list (may create VBOs, textures).
	void render(
		Program* program,
		const rr::RRObject* object,
		const FaceGroupRange* faceGroupRange,
		unsigned numFaceGroupRanges,
		const UberProgramSetup& uberProgramSetup,
		bool _renderingFromLight, // true = renders also invisible backfaces (!renderFrom) that block photons (catchFrom)
		rr::RRBuffer* lightIndirectBuffer,
		const rr::RRBuffer* lightDetailMap);
};

}; // namespace

#endif
