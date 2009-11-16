// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

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

RendererOfRRObject::RendererOfRRObject(const rr::RRObject* _object, rr::RRDynamicSolver* _solver)
{
	RR_ASSERT(_object);
	params.program = NULL;
	params.object = _object;
	params.scene = _solver;
	//params.renderedChannels = ... set to default by constructor
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
	params.lightIndirectVersion = UINT_MAX;
}

RendererOfRRObject::~RendererOfRRObject()
{
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

	bool containsBlended;
	bool containsNonBlended;
	params.object->faceGroups.getBlending(containsBlended,containsNonBlended);
	return (_renderNonBlended && containsNonBlended) || (_renderBlended && containsBlended);
}

void RendererOfRRObject::setCapture(unsigned _firstCapturedTriangle, unsigned _lastCapturedTrianglePlus1)
{
	params.firstCapturedTriangle = _firstCapturedTriangle;
	params.lastCapturedTrianglePlus1 = _lastCapturedTrianglePlus1;
}

void RendererOfRRObject::setIndirectIlluminationBuffers(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap, unsigned _lightIndirectVersion)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	params.lightIndirectVersion = _lightIndirectVersion;
}

void RendererOfRRObject::setIndirectIlluminationBuffersBlend(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap, rr::RRBuffer* vertexBuffer2, const rr::RRBuffer* ambientMap2, unsigned _lightIndirectVersion)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = vertexBuffer2;
	params.availableIndirectIlluminationMap2 = ambientMap2;
	params.lightIndirectVersion = _lightIndirectVersion;
}

void RendererOfRRObject::setIndirectIlluminationFromSolver(unsigned _lightIndirectVersion)
{
	params.indirectIlluminationSource = SOLVER;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	params.lightIndirectVersion = _lightIndirectVersion;
}

void RendererOfRRObject::setLDM(const rr::RRBuffer* ldmBuffer)
{
	params.availableIndirectIlluminationLDMap = ldmBuffer;
}

void RendererOfRRObject::setLightingShadowingFlags(const rr::RRLight* renderingFromThisLight, const rr::RRLight* renderingLitByThisLight)
{
	params.renderingFromThisLight = renderingFromThisLight;
	params.renderingLitByThisLight = renderingLitByThisLight;
}

}; // namespace
