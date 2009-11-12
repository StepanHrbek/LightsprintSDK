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
// 1x1 textures

static rr::RRBuffer* buffer1x1[5] = {NULL,NULL,NULL,NULL,NULL};

static void bindPropertyTexture(const rr::RRMaterial::Property& property,unsigned index)
{
	rr::RRBuffer* buffer = property.texture;
	if (!buffer)
	{
		if (!buffer1x1[index])
			buffer1x1[index] = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,(index==2)?rr::BF_RGBA:rr::BF_RGB,true,NULL); // 2 = RGBA
		buffer1x1[index]->setElement(0,rr::RRVec4(property.color,1-property.color.avg()));
		buffer = buffer1x1[index];
	}
	getTexture(buffer)->bindTexture();
}

static void free1x1()
{
	for (unsigned i=0;i<5;i++)
		RR_SAFE_DELETE(buffer1x1[i]);
}

unsigned g_numRenderers = 0;


//////////////////////////////////////////////////////////////////////////////
//
// RenderedChannels

void RendererOfRRObject::RenderedChannels::useMaterial(Program* program, const rr::RRMaterial* material)
{
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useMaterial(): program=NULL\n"));
		return;
	}
	if (!material)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useMaterial(): material=NULL\n"));
		rr::RRMaterial s_material;
		RR_LIMITED_TIMES(1,s_material.reset(false));
		material = &s_material;
	}
	if (MATERIAL_DIFFUSE_CONST)
	{
		program->sendUniform("materialDiffuseConst",material->diffuseReflectance.color[0],material->diffuseReflectance.color[1],material->diffuseReflectance.color[2],1.0f);
	}

	if (MATERIAL_SPECULAR_CONST)
	{
		program->sendUniform("materialSpecularConst",material->specularReflectance.color[0],material->specularReflectance.color[1],material->specularReflectance.color[2],1.0f);
	}

	if (MATERIAL_EMISSIVE_CONST)
	{
		program->sendUniform("materialEmissiveConst",material->diffuseEmittance.color[0],material->diffuseEmittance.color[1],material->diffuseEmittance.color[2],0.0f);
	}

	if (MATERIAL_TRANSPARENCY_CONST)
	{
		program->sendUniform("materialTransparencyConst",material->specularTransmittance.color[0],material->specularTransmittance.color[1],material->specularTransmittance.color[2],1-material->specularTransmittance.color.avg());
	}

	if (MATERIAL_DIFFUSE_MAP)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_DIFFUSE);
		bindPropertyTexture(material->diffuseReflectance,0);
	}

	if (MATERIAL_EMISSIVE_MAP)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_EMISSIVE);
		bindPropertyTexture(material->diffuseEmittance,1);
	}

	if (MATERIAL_TRANSPARENCY_MAP)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_TRANSPARENCY);
		bindPropertyTexture(material->specularTransmittance,2); // 2 = RGBA
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject

RendererOfRRObject* RendererOfRRObject::create(const rr::RRObject* object, rr::RRDynamicSolver* solver)
{
	if (object && object->getCollider()->getMesh()->getNumTriangles())
		return new RendererOfRRObject(object,solver);
	else
		return NULL;
}

RendererOfRRObject::RendererOfRRObject(const rr::RRObject* _object, rr::RRDynamicSolver* _solver)
{
	g_numRenderers++;
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

	lightIndirectVersion = UINT_MAX;
	containsNonBlended = 1; // init to anything, ObjectBuffers::create resets it
	containsBlended = 0; // init to anything, ObjectBuffers::create resets it
	unwrapSplitsVertices = 0; // init to 0, ObjectBuffers::create ors it
	indexedYes = ObjectBuffers::create(_object,true,containsNonBlended,containsBlended,unwrapSplitsVertices);
	indexedNo = ObjectBuffers::create(_object,false,containsNonBlended,containsBlended,unwrapSplitsVertices);
}

RendererOfRRObject::~RendererOfRRObject()
{
	delete indexedNo;
	delete indexedYes;
	if (!--g_numRenderers) free1x1();
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
	lightIndirectVersion = _lightIndirectVersion;
}

void RendererOfRRObject::setIndirectIlluminationBuffersBlend(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap, rr::RRBuffer* vertexBuffer2, const rr::RRBuffer* ambientMap2, unsigned _lightIndirectVersion)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = vertexBuffer2;
	params.availableIndirectIlluminationMap2 = ambientMap2;
	lightIndirectVersion = _lightIndirectVersion;
}

void RendererOfRRObject::setIndirectIlluminationFromSolver(unsigned _lightIndirectVersion)
{
	params.indirectIlluminationSource = SOLVER;
	params.indirectIlluminationBlend = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	lightIndirectVersion = _lightIndirectVersion;
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
	bool readIndirectFromNone = renderIndirect && params.indirectIlluminationSource==NONE;

	bool setNormals = params.renderedChannels.NORMALS || params.renderedChannels.LIGHT_DIRECT;

	// BUFFERS
	// general and faster code, but can't handle objects with big preimport vertex numbers (e.g. multiobject)
	// -> automatic fallback to !BUFFERS

	bool dontUseIndexed = 0;
	bool dontUseNonIndexed = 0;
	bool dontUseIndexedBecauseOfUnwrap = 0;

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
	// unwrap may be incompatible with indexed render (fallback uv generated by us)
	if ((params.renderedChannels.LIGHT_INDIRECT_MAP || params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP) && unwrapSplitsVertices)
	{
		dontUseIndexedBecauseOfUnwrap = true;
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

	if (indexedYes && !dontUseIndexed && !dontUseIndexedBecauseOfUnwrap)
	{
		// INDEXED
		// indexed is faster, use always when possible
		indexedYes->render(params,lightIndirectVersion);
	}
	else
	if (indexedNo && !dontUseNonIndexed)
	{
		// NON INDEXED
		// non-indexed is slower
		indexedNo->render(params,lightIndirectVersion);
	}
	else
	if (indexedYes && !dontUseIndexed)
	{
		// INDEXED
		// with unwrap errors
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Unwrap may be rendered incorrectly (circumstances: %s%s%s%s%s%s%s).\n", unwrapSplitsVertices?"unwrapSplitsVertices+":"", readIndirectFromBuffers?"renderingFromBuffers+":"", readIndirectFromSolver?"readIndirectFromSolver+":"", params.renderedChannels.LIGHT_INDIRECT_VCOLOR?"LIGHT_INDIRECT_VCOLOR+":"", params.renderedChannels.LIGHT_INDIRECT_MAP?"LIGHT_INDIRECT_MAP+":"", params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP?"LIGHT_INDIRECT_DETAIL_MAP":"", params.renderedChannels.FORCE_2D_POSITION?"FORCE_2D_POSITION":"" ));
		indexedYes->render(params,lightIndirectVersion);
	}
	else
	{
		// NONE
		// none of rendering techniques can do this job
		if (indexedYes||indexedNo)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Scene is not rendered (circumstances: %s%s%s%s%s%s%s%s%s).\n", indexedYes?"indexed+":"", indexedNo?"nonindexed":"", unwrapSplitsVertices?"unwrapSplitsVertices+":"", readIndirectFromBuffers?"renderingFromBuffers+":"", readIndirectFromSolver?"readIndirectFromSolver+":"", params.renderedChannels.LIGHT_INDIRECT_VCOLOR?"LIGHT_INDIRECT_VCOLOR+":"", params.renderedChannels.LIGHT_INDIRECT_MAP?"LIGHT_INDIRECT_MAP+":"", params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP?"LIGHT_INDIRECT_DETAIL_MAP":"", params.renderedChannels.FORCE_2D_POSITION?"FORCE_2D_POSITION":"" ));
		}
		else
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Scene is not rendered (not enough memory).\n" ));
		}
	}
}

}; // namespace
