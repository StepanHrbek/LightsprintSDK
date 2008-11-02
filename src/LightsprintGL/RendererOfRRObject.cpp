// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/GL/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "ObjectBuffers.h"
#include "PreserveState.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject

RendererOfRRObject* RendererOfRRObject::create(const rr::RRObject* object, rr::RRDynamicSolver* solver, const rr::RRScaler* scaler, bool useBuffers)
{
	if (object && object->getCollider()->getMesh()->getNumTriangles())
		return new RendererOfRRObject(object,solver,scaler,useBuffers);
	else
		return NULL;
}

RendererOfRRObject::RendererOfRRObject(const rr::RRObject* _object, rr::RRDynamicSolver* _solver, const rr::RRScaler* _scaler, bool _useBuffers)
{
	RR_ASSERT(_object);
	params.program = NULL;
	params.object = _object;
	params.scene = _solver;
	params.scaler = _scaler;
	//params.renderedChannels = ... set to default by constructor
	params.generateForcedUv = NULL;
	params.firstCapturedTriangle = 0;
	params.lastCapturedTrianglePlus1 = _object->getCollider()->getMesh()->getNumTriangles();
	params.indirectIlluminationSource = NONE;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	params.availableIndirectIlluminationLDMap = NULL;
	params.renderingFromThisLight = NULL;
	params.renderingLitByThisLight = NULL;
	params.renderBlended = true;
	params.renderNonBlended = true;

	indexedYes = NULL;
	indexedNo = NULL;
	containsNonBlended = 1;
	containsBlended = 0;
	if (_useBuffers)
	{
		indexedYes = ObjectBuffers::create(_object,true,containsNonBlended,containsBlended);
		indexedNo = ObjectBuffers::create(_object,false,containsNonBlended,containsBlended);
	}
}

RendererOfRRObject::~RendererOfRRObject()
{
	delete indexedNo;
	delete indexedYes;
}

void RendererOfRRObject::setProgram(Program* program)
{
	params.program = program;
}

void RendererOfRRObject::setRenderedChannels(RenderedChannels renderedChannels)
{
	params.renderedChannels = renderedChannels;
}

bool RendererOfRRObject::setMaterialFilter(bool _renderNonBlended, bool _renderBlended)
{
	params.renderNonBlended = _renderNonBlended;
	params.renderBlended = _renderBlended;
	return (_renderNonBlended && containsNonBlended) || (_renderBlended && containsBlended);
}

void RendererOfRRObject::setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle, unsigned alastCapturedTrianglePlus1)
{
	params.generateForcedUv = capture;
	params.otherCaptureParamsHash = capture ? capture->getHash() : 0;
	params.firstCapturedTriangle = afirstCapturedTriangle;
	params.lastCapturedTrianglePlus1 = alastCapturedTrianglePlus1;
}

void RendererOfRRObject::setIndirectIlluminationBuffers(rr::RRBuffer* vertexBuffer,const rr::RRBuffer* ambientMap)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	solutionVersion = 0;
	getTexture(ambientMap,true,false); // prebuild texture so we don't do it in display list (probably legal, but triggers AMD driver bug). And don't compres lmaps(ugly 4x4 blocks on HD2400)
}

void RendererOfRRObject::setIndirectIlluminationBuffersBlend(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap, rr::RRBuffer* vertexBuffer2, const rr::RRBuffer* ambientMap2)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = vertexBuffer2;
	params.availableIndirectIlluminationMap2 = ambientMap2;
	solutionVersion = 0;
	getTexture(ambientMap,true,false); // prebuild texture so we don't do it in display list (probably legal, but triggers AMD driver bug). And don't compres lmaps(ugly 4x4 blocks on HD2400)
	getTexture(ambientMap2,true,false); // prebuild texture so we don't do it in display list (probably legal, but triggers AMD driver bug). And don't compres lmaps(ugly 4x4 blocks on HD2400)
}

void RendererOfRRObject::setIndirectIlluminationFromSolver(unsigned asolutionVersion)
{
	params.indirectIlluminationSource = SOLVER;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	solutionVersion = asolutionVersion;
}

void RendererOfRRObject::setLDM(const rr::RRBuffer* ldmBuffer)
{
	params.availableIndirectIlluminationLDMap = ldmBuffer;
	getTexture(ldmBuffer); // prebuild texture so we don't do it in display list (probably legal, but triggers AMD driver bug)
}

void RendererOfRRObject::setLightingShadowingFlags(const rr::RRLight* renderingFromThisLight, const rr::RRLight* renderingLitByThisLight)
{
	params.renderingFromThisLight = renderingFromThisLight;
	params.renderingLitByThisLight = renderingLitByThisLight;
}

const void* RendererOfRRObject::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RendererOfRRObject::render()
{
	RR_ASSERT(params.object);
	if (!params.object) return;

	// vcolor2 allowed only if vcolor is set
	RR_ASSERT(params.renderedChannels.LIGHT_INDIRECT_VCOLOR || !params.renderedChannels.LIGHT_INDIRECT_VCOLOR2);
	// map2 allowed only if map is set
	RR_ASSERT(params.renderedChannels.LIGHT_INDIRECT_MAP || !params.renderedChannels.LIGHT_INDIRECT_MAP2);
	// light detail map allowed only if map is not set
	RR_ASSERT(!params.renderedChannels.LIGHT_INDIRECT_MAP || !params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP);

	// indirect illumination source - solver, buffers or none?
	bool renderIndirect = params.renderedChannels.LIGHT_INDIRECT_VCOLOR || params.renderedChannels.LIGHT_INDIRECT_MAP;
	bool readIndirectFromSolver = renderIndirect && params.indirectIlluminationSource==SOLVER;
	bool readIndirectFromBuffers = renderIndirect && params.indirectIlluminationSource==BUFFERS;
	bool readIndirectFromLayer = renderIndirect && params.indirectIlluminationSource==LAYER;
	bool readIndirectFromNone = renderIndirect && params.indirectIlluminationSource==NONE;

	bool setNormals = params.renderedChannels.NORMALS || params.renderedChannels.LIGHT_DIRECT;

	// BUFFERS
	// general and faster code, but can't handle objects with big preimport vertex numbers (e.g. multiobject)
	// -> automatic fallback to !BUFFERS

	bool dontUseIndexed = 0;
	bool dontUseNonIndexed = 0;

	// buffer generated by RRDynamicSolver need original (preimport) vertex order, it is only in indexed
	if (readIndirectFromBuffers && params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		dontUseNonIndexed = true;
	}
	// ObjectBuffers can't create indexed buffer from solver
	if (readIndirectFromSolver)
	{
		dontUseIndexed = true;
	}
	// ambient maps are sometimes incompatible with indexed render (fallback uv generated by us)
	//!!! if possible, ask RRMesh about mapping/unwrap compatibility with indexed render. our unwrap is incompatible, others should be compatible
	if (readIndirectFromBuffers && params.renderedChannels.LIGHT_INDIRECT_MAP)
	{
		dontUseIndexed = true;
	}
	// indirectFromSolver can't generate ambient maps
	if (readIndirectFromSolver && params.renderedChannels.LIGHT_INDIRECT_MAP)
	{
		RR_ASSERT(0);
		return;
	}
	// FORCE_2D_POSITION vertices must be unique, incompatible with indexed buffer
	if (params.renderedChannels.FORCE_2D_POSITION)
	{
		dontUseIndexed = true;
	}
	// only nonbuffered can read from arbitrary layer
	if (readIndirectFromLayer)
	{
		dontUseIndexed = true;
		dontUseNonIndexed = true;
	}
	// error, you forgot to call setIndirectIlluminationXxx()
	if (readIndirectFromNone)
	{
		RR_ASSERT(0);
		return;
	}

	/////////////////////////////////////////////////////////////////////////

	PreserveCullFace p1;
	PreserveCullMode p2;
	PreserveBlend p3;

	glColor4ub(0,0,0,255);

	/////////////////////////////////////////////////////////////////////////

	if (indexedYes && !dontUseIndexed)
	{
		// INDEXED
		// indexed is faster, use always when possible
		indexedYes->render(params,solutionVersion);
	}
	else
	if (indexedNo && !dontUseNonIndexed)
	{
		// NON INDEXED
		// non-indexed is slower
		indexedNo->render(params,solutionVersion);
	}
	else
	{
		// NONE
		// none of rendering techniques can do this job
		RR_ASSERT(0);
	}
}

}; // namespace
