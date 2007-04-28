// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RENDEREROFRROBJECT_H
#define RENDEREROFRROBJECT_H

#include <cstring> // memset
#include "Lightsprint/RRObject.h"
#include "Lightsprint/DemoEngine/Renderer.h"
#include "Lightsprint/RRGPUOpenGL.h"

namespace rr_gl
{

// Custom channels supported by RendererOfRRObject.
// (they allocate channel numbers not used by Lightsprint engine)
enum
{
	CHANNEL_TRIANGLE_DIFFUSE_TEX         = rr::RRMesh::INDEXED_BY_TRIANGLE+5, ///< channel contains Texture* for each triangle
	CHANNEL_TRIANGLE_EMISSIVE_TEX        = rr::RRMesh::INDEXED_BY_TRIANGLE+6, ///< channel contains Texture* for each triangle
	CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV = rr::RRMesh::INDEXED_BY_TRIANGLE+7, ///< channel contains RRVec2[3] for each triangle
	CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV= rr::RRMesh::INDEXED_BY_TRIANGLE+8, ///< channel contains RRVec2[3] for each triangle
	CHANNEL_TRIANGLE_OBJECT_ILLUMINATION = rr::RRMesh::INDEXED_BY_TRIANGLE+9, ///< channel contains RRObjectIllumination* for each triangle
};


//////////////////////////////////////////////////////////////////////////////
//
// per vertex data generator

//! Generator of per vertex data, for internal use.
class VertexDataGenerator
{
public:
	virtual ~VertexDataGenerator() {};
	// generates vertex data into 'size' bytes of 'vertexData'
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) = 0; // vertexIndex=0..2
	// returns hash of all parameters that modify generateData behaviour.
	virtual unsigned getHash() = 0;
};


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject

//! OpenGL renderer of 3d object with RRObject interface.
//
//! Renders RRObject instance.
//! Feeds OpenGL with primitives and binds textures,
//! expects that shaders were already set.
//! 
//! Why does it exist?
//! - It can render each triangle with independent generated uv coords,
//!   without regard to other triangles that share the same vertex.
//! - This special feature is useful during detectDirectIllumination()
//!   and for ambient mapped render with autogenerated ambient map unwrap.
//!
//! Other ways how to reach the same without second renderer:
//! - Stay with any other renderer you have.
//!   Don't share vertices, resave all your meshes with vertices splitted.
//!   This makes mesh bigger and rendering bit slower,
//!   but uv coord may be set for each triangle independently.
//!   You can eventually store both versions of mesh and render
//!   the bigger one only during detectDirectIllumination().
//! - Stay with any other renderer that supports geometry shaders.
//!   Write simple geometry shader that generates uv.
class RR_API RendererOfRRObject : public de::Renderer
{
public:
	//! Creates renderer of object.
	//! \param object
	//!  Object to be rendered.
	//! \param radiositySolver
	//!  Solver used to compute object's indirect illumination.
	//! \param scaler
	//!  Scaler used to scale irradiances before rendering from physical scale to custom scale.
	//! \param useBuffers
	//!  Set true for rendering technique with vertex buffers. Makes rendering faster,
	//!  but only if it's done many times repeatedly. More memory is needed.
	//!  If you plan to render object only once,
	//!  technique without buffers (false) is faster and takes less memory.
	//!  \n Technique without buffers doesn't support setIndirectIlluminationBuffers().
	RendererOfRRObject(const rr::RRObject* object, const rr::RRStaticSolver* radiositySolver, const rr::RRScaler* scaler, bool useBuffers);

	//! Specifies what data channels to feed to GPU during render.
	struct RenderedChannels
	{
		bool     LIGHT_DIRECT           :1; ///< feeds gl_Normal
		bool     LIGHT_INDIRECT_VCOLOR  :1; ///< feeds gl_Color. Could be read from RRStaticSolver or RRObjectIllumination or RRIlluminationVertexBuffer, to be better specified later.
		bool     LIGHT_INDIRECT_MAP     :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_LIGHT_INDIRECT] + texture[TEXTURE_2D_LIGHT_INDIRECT]. Could be read from RRObjectIllumination or RRIlluminationPixelBuffer, to be better specified later.
		bool     LIGHT_INDIRECT_ENV     :1; ///< feeds gl_Normal + texture[TEXTURE_CUBE_LIGHT_INDIRECT]. Always read from RRObjectIllumination.
		bool     MATERIAL_DIFFUSE_VCOLOR:1; ///< feeds gl_SecondaryColor
		bool     MATERIAL_DIFFUSE_MAP   :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_MATERIAL_DIFFUSE] + texture[TEXTURE_2D_MATERIAL_DIFFUSE]
		bool     MATERIAL_EMISSIVE_MAP  :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_MATERIAL_EMISSIVE] + texture[TEXTURE_2D_MATERIAL_EMISSIVE]
		bool     FORCE_2D_POSITION      :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_FORCED_2D]
		unsigned LIGHT_MAP_LAYER;         ///< if LIGHT_INDIRECT_MAP, maps from this illuminations channel are used
		//! Creates setup with everything off, only vertex positions are rendered.
		//! Suitable for rendering into shadowmaps.
		RenderedChannels()
		{
			memset(this,0,sizeof(*this));
		}
	};

	//! Sets what data channels to feed to GPU during render().
	void setRenderedChannels(RenderedChannels renderedChannels);

	//! Sets source of uv coords for render() with FORCE_2D_POSITION enabled.
	void setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle, unsigned alastCapturedTrianglePlus1);

	//! Specifies what indirect illumination to render in render(): use these buffers.
	//
	//! Overrides previous calls to setIndirectIlluminationBuffers() and setIndirectIlluminationFromSolver().
	//! It is not supported in combination with useBuffers=false (set at renderer creation time).
	//! \param vertexBuffer
	//!  Used by render() with LIGHT_INDIRECT_VCOLOR.
	//!  Vertex buffer with indirect illumination colors. Must be of the same size as the object.
	//!  Order of values in vertex buffer is defined by object's preimport vertex numbers
	//!  (see RRMesh and RRMesh::getPreImportVertex()).
	//! \param ambientMap
	//!  Used by render() with LIGHT_INDIRECT_MAP.
	//!  Ambient map with indirect illumination values.
	//!  Texcoord mapping is provided by RRMesh::getTriangleMapping().
	void setIndirectIlluminationBuffers(rr::RRIlluminationVertexBuffer* vertexBuffer,const rr::RRIlluminationPixelBuffer* ambientMap);

	//! Specifies what indirect illumination to render in render(): read live values from the solver.
	//
	//! Overrides previous calls to setIndirectIlluminationBuffers() and setIndirectIlluminationFromSolver().
	//! \param solutionVersion
	//!  If you change this number, indirect illumination data are read form the solver at render() time.
	//!  If you call render() without changing this number,
	//!  indirect illumination from previous render() is reused and render is faster.
	//!  RRDynamicSolver::getSolutionVersion() is usually entered here.
	void setIndirectIlluminationFromSolver(unsigned solutionVersion);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Renders object, feeds OpenGL with object's data selected by setRenderedChannels().
	//! Doesn't set any shader, expects that it was already set by caller.
	virtual void render();

	virtual ~RendererOfRRObject();

private:
	friend class ObjectBuffers;
	struct Params
	{
		const rr::RRObject* object;            ///< object being rendered
		const rr::RRStaticSolver* scene;       ///< scene it comes from
		const rr::RRScaler* scaler;            ///< scaler used to translate physical to custom irradiance when LIGHT_INDIRECT_VCOLOR
		RenderedChannels renderedChannels;     ///< set of data channels being rendered
		// set by setCapture()
		VertexDataGenerator* generateForcedUv; ///< generator of uv data for FORCE_2D_POSITION
		unsigned otherCaptureParamsHash;       ///< hash of generator's parameters
		unsigned firstCapturedTriangle;        ///< index of first triangle to render
		unsigned lastCapturedTrianglePlus1;    ///< index of last triangle to render+1
		// set by setIndirectIlluminationXxx()
		bool availableIndirectIlluminationSolver; ///< if true, read indirect illumination from solver, rather than from following buffers
		rr::RRIlluminationVertexBuffer* availableIndirectIlluminationVColors; ///< vertex buffer with indirect illumination (not const because lock is not const)
		const rr::RRIlluminationPixelBuffer* availableIndirectIlluminationMap; ///< ambient map
	};
	Params params;
	// buffers for faster rendering
	class ObjectBuffers* indexedYes;
	class ObjectBuffers* indexedNo;
	unsigned solutionVersion;              ///< Version of solution in static solver. Must not be in Params, because it changes often and would cause lots of displaylist rebuilds.
};

}; // namespace

#endif
