//----------------------------------------------------------------------------
//! \file RendererOfRRObject.h
//! \brief LightsprintGL | renderers RRObject instance
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDEREROFRROBJECT_H
#define RENDEREROFRROBJECT_H

#include <cstring> // memset
#include "Lightsprint/RRObject.h"
#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

namespace rr_gl
{

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
//!   Implement detectDirectIllumination() using simple geometry shader that generates uv.
class RR_GL_API RendererOfRRObject : public Renderer
{
public:
	//! Creates renderer of object.
	//! \param object
	//!  Object to be rendered.
	//! \param solver
	//!  Solver used to compute object's indirect illumination.
	//! \param scaler
	//!  Scaler used to scale irradiances before rendering from physical scale to custom scale.
	//! \param useBuffers
	//!  Set true for rendering technique with vertex buffers. Makes rendering faster,
	//!  but only if it's done many times repeatedly. More memory is needed.
	//!  If you plan to render object only once,
	//!  technique without buffers (false) is faster and takes less memory.
	//!  \n Technique without buffers doesn't support setIndirectIlluminationBuffers().
	static RendererOfRRObject* create(const rr::RRObject* object, rr::RRDynamicSolver* solver, const rr::RRScaler* scaler, bool useBuffers);

	//! Specifies what data channels to feed to GPU during render.
	struct RenderedChannels
	{
		bool     NORMALS                :1; ///< feeds gl_Normal
		bool     LIGHT_DIRECT           :1; ///< feeds gl_Normal
		bool     LIGHT_INDIRECT_VCOLOR  :1; ///< feeds gl_Color. Read from RRStaticSolver or RRObjectIllumination or RRBuffer.
		bool     LIGHT_INDIRECT_VCOLOR2 :1; ///< feeds gl_SecondaryColor
		bool     LIGHT_INDIRECT_MAP     :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_LIGHT_INDIRECT] + texture[TEXTURE_2D_LIGHT_INDIRECT]. Read from RRObjectIllumination or RRBuffer.
		bool     LIGHT_INDIRECT_MAP2    :1; ///< feeds texture[TEXTURE_2D_LIGHT_INDIRECT2]
		bool     LIGHT_INDIRECT_ENV     :1; ///< feeds gl_Normal + texture[TEXTURE_CUBE_LIGHT_INDIRECT]. Always read from RRObjectIllumination.
		bool     MATERIAL_DIFFUSE_CONST :1; ///< feeds uniform materialDiffuseConst
		bool     MATERIAL_DIFFUSE_VCOLOR:1; ///< feeds materialDiffuseVColor
		bool     MATERIAL_DIFFUSE_MAP   :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_MATERIAL_DIFFUSE] + texture[TEXTURE_2D_MATERIAL_DIFFUSE]
		bool     MATERIAL_EMISSIVE_CONST:1; ///< feeds uniform materialEmissiveConst
		bool     MATERIAL_EMISSIVE_VCOLOR:1;///< feeds gl_MultiTexCoord[MULTITEXCOORD_MATERIAL_EMISSIVE_VCOLOR]
		bool     MATERIAL_EMISSIVE_MAP  :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_MATERIAL_EMISSIVE] + texture[TEXTURE_2D_MATERIAL_EMISSIVE]
		bool     MATERIAL_TRANSPARENT   :1; ///< sets transparency according to material (0=defaults in GL pipeline are used)
		bool     MATERIAL_CULLING       :1; ///< sets 1/2-sided face according to material (0=defaults in GL pipeline are used)
		bool     FORCE_2D_POSITION      :1; ///< feeds gl_MultiTexCoord[MULTITEXCOORD_FORCED_2D]
		//! Creates setup with everything off, only vertex positions are rendered.
		//! Suitable for rendering into shadowmaps.
		RenderedChannels()
		{
			memset(this,0,sizeof(*this));
		}
	};

	//! Sets what program to use for changing uniform colors. (This class doesn't bind it, only changes some uniforms.)
	void setProgram(Program* program);

	//! Sets what data channels to feed to GPU during render().
	void setRenderedChannels(RenderedChannels renderedChannels);

	//! Sets source of uv coords for render() with FORCE_2D_POSITION enabled.
	void setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle, unsigned alastCapturedTrianglePlus1);

	//! Specifies what indirect illumination to render in render(): use these buffers.
	//
	//! Overrides previous calls to setIndirectIlluminationBuffers(), setIndirectIlluminationLayer() and setIndirectIlluminationFromSolver().
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
	void setIndirectIlluminationBuffers(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap);

	//! Specifies what indirect illumination to render in render(): use blend of these buffers.
	//
	//! This is setIndirectIlluminationBuffers() extended with second set of data,
	//! both sets are pushed into OpenGL pipeline at render time.
	void setIndirectIlluminationBuffersBlend(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap, rr::RRBuffer* vertexBuffer2, const rr::RRBuffer* ambientMap2);

	//! Specifies what indirect illumination to render in render(): use buffers from this layer.
	//
	//! Overrides previous calls to setIndirectIlluminationBuffers(), setIndirectIlluminationLayer() and setIndirectIlluminationFromSolver().
	//! \param layerNumber
	//!  Number of layer to take indirect illumination buffers from.
	void setIndirectIlluminationLayer(unsigned layerNumber);

	//! Specifies what indirect illumination to render in render(): use blend of these layers.
	//
	//! \param layerNumber
	//!  Number of first source layer.
	//! \param layerNumber2
	//!  Number of second source layer.
	//! \param layerBlend
	//!  Data from both layers are blended at render time, first layer * (1-layerBlend + second layer * layerBlend.
	//! \param layerNumberFallback
	//!  When previous layers contain no data, data from this layer are used.
	void setIndirectIlluminationLayerBlend(unsigned layerNumber, unsigned layerNumber2, float layerBlend, unsigned layerNumberFallback);

	//! Specifies what indirect illumination to render in render(): read live values from the solver.
	//
	//! Overrides previous calls to setIndirectIlluminationBuffers(), setIndirectIlluminationLayer() and setIndirectIlluminationFromSolver().
	//! \param solutionVersion
	//!  If you change this number, indirect illumination data are read form the solver at render() time.
	//!  If you call render() without changing this number,
	//!  indirect illumination from previous render() is reused and render is faster.
	//!  RRDynamicSolver::getSolutionVersion() is usually entered here.
	void setIndirectIlluminationFromSolver(unsigned solutionVersion);

	//! Enables lighting and shadowing flag tests.
	//
	//! When lights are not specified or set to NULL, all objects are lit and shadows rendered as in real world.
	//! When lights are specified, lighting and shadowing flags are tested (see rr::RRObject::getTriangleMaterial())
	//! more or less accurately, depending on honourExpensiveLightingShadowingFlags.
	//!
	//! In other words:
	//! \n Before rendering shadow casters into shadowmap, set light and renderingShadowCasters,
	//! triangles that don't cast shadows won't be renderd.
	//! \n Before rendering light receivers, set light and unset renderingShadowCasters,
	//! triangles that don't receive light won't be renderd.
	//! \n In other cases, pass light=NULL, all triangles will be rendered.
	void setLightingShadowingFlags(const rr::RRLight* renderingFromThisLight, const rr::RRLight* renderingLitByThisLight, bool honourExpensiveLightingShadowingFlags);

	//! Returns parameters with influence on render().
	virtual const void* getParams(unsigned& length) const;

	//! Renders object, feeds OpenGL with object's data selected by setRenderedChannels().
	//! Doesn't set any shader, expects that it was already set by caller.
	virtual void render();

	virtual ~RendererOfRRObject();

private:
	friend class ObjectBuffers;
	RendererOfRRObject(const rr::RRObject* object, rr::RRDynamicSolver* radiositySolver, const rr::RRScaler* scaler, bool useBuffers);
	enum IndirectIlluminationSource
	{
		NONE,
		BUFFERS,
		LAYER,
		SOLVER,
	};
	struct Params
	{
		Program* program;                      ///< current program, used only by MATERIAL_DIFFUSE_CONST, MATERIAL_EMISSIVE_CONST
		const rr::RRObject* object;            ///< object being rendered
		rr::RRDynamicSolver* scene;            ///< scene it comes from
		const rr::RRScaler* scaler;            ///< scaler used to translate physical to custom irradiance when LIGHT_INDIRECT_VCOLOR
		RenderedChannels renderedChannels;     ///< set of data channels being rendered
		// set by setCapture()
		VertexDataGenerator* generateForcedUv; ///< generator of uv data for FORCE_2D_POSITION
		unsigned otherCaptureParamsHash;       ///< hash of generator's parameters
		unsigned firstCapturedTriangle;        ///< index of first triangle to render
		unsigned lastCapturedTrianglePlus1;    ///< index of last triangle to render+1
		// set by setIndirectIlluminationXxx()
		IndirectIlluminationSource indirectIlluminationSource;
		unsigned indirectIlluminationLayer;
		unsigned indirectIlluminationLayer2;
		float indirectIlluminationBlend;
		unsigned indirectIlluminationLayerFallback;
		rr::RRBuffer* availableIndirectIlluminationVColors; ///< vertex buffer with indirect illumination (not const because lock is not const)
		rr::RRBuffer* availableIndirectIlluminationVColors2;
		const rr::RRBuffer* availableIndirectIlluminationMap; ///< ambient map
		const rr::RRBuffer* availableIndirectIlluminationMap2;
		// set by setLightingShadowingFlags()
		const rr::RRLight* renderingFromThisLight;
		const rr::RRLight* renderingLitByThisLight;
		bool honourExpensiveLightingShadowingFlags;
	};
	Params params;
	// buffers for faster rendering
	class ObjectBuffers* indexedYes;
	class ObjectBuffers* indexedNo;
	unsigned solutionVersion;              ///< Version of solution in static solver. Must not be in Params, because it changes often and would cause lots of displaylist rebuilds.
};

}; // namespace

#endif
