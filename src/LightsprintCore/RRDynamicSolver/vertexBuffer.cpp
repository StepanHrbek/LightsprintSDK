// --------------------------------------------------------------------------
// Offline and realtime per-vertex lightmaps.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "Lightsprint/RRDynamicSolver.h"
#include "report.h"
#include "private.h"
#include "gather.h"
#include "../RRStaticSolver/rrcore.h"

namespace rr
{

void RRDynamicSolver::updateVertexLookupTableDynamicSolver()
// prepare lookup tables preImportVertex -> [postImportTriangle,vertex012] for all objects
{
	RRReportInterval reportProp(INF3,"Updating vertex lookup table...\n");
	if(!getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	// allocate and clear table
	RR_ASSERT(priv->preVertex2PostTriangleVertex.empty()); // full clear to UNDEFINED. without full clear, invalid values would stay alive in vertices we don't overwrite (e.g. needles)
	priv->preVertex2PostTriangleVertex.resize(priv->objects.size());
	for(unsigned objectHandle=0;objectHandle<priv->objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		if(illumination)
		{
			unsigned numPreImportSingleVertices = illumination->getNumPreImportVertices();
			priv->preVertex2PostTriangleVertex[objectHandle].resize(numPreImportSingleVertices,Private::TriangleVertexPair(RRMesh::UNDEFINED,RRMesh::UNDEFINED));
		}
	}
	// fill table
	const RRMesh* multiMesh = getMultiObjectPhysical()->getCollider()->getMesh();
	unsigned numPostImportMultiVertices = multiMesh->getNumVertices();
	unsigned numPostImportMultiTriangles = multiMesh->getNumTriangles();
	for(unsigned postImportMultiTriangle=0;postImportMultiTriangle<numPostImportMultiTriangles;postImportMultiTriangle++)
	{
		RRMesh::Triangle postImportMultiTriangleVertices;
		multiMesh->getTriangle(postImportMultiTriangle,postImportMultiTriangleVertices);
		for(unsigned v=0;v<3;v++)
		{
			unsigned postImportMultiVertex = postImportMultiTriangleVertices[v];
			if(postImportMultiVertex<numPostImportMultiVertices)
			{
				RRMesh::PreImportNumber preVertexMulti = multiMesh->getPreImportVertex(postImportMultiVertex,postImportMultiTriangle);
				if(priv->scene && !priv->scene->scene->object->triangle[postImportMultiTriangle].topivertex[v])
				{
					// static solver doesn't like this triangle and set surface NULL, probably because it is a needle
					// let it UNDEFINED
				}
				else
				{
					priv->preVertex2PostTriangleVertex[preVertexMulti.object][preVertexMulti.index] = Private::TriangleVertexPair(postImportMultiTriangle,v);
				}
			}
			else
			{
				// should not get here. let it UNDEFINED
				RR_ASSERT(0);
			}
		}
	}
}

void RRDynamicSolver::updateVertexLookupTablePackedSolver()
// prepare lookup tables preImportVertex -> Ivertex for multiobject and for all singleobjects
{
	RRReportInterval reportProp(INF3,"Updating fireball lookup table...\n");
	if(!priv->packedSolver || !getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	const RRMesh* multiMesh = getMultiObjectPhysical()->getCollider()->getMesh();
	unsigned numPostImportMultiVertices = multiMesh->getNumVertices();
	unsigned numPostImportMultiTriangles = multiMesh->getNumTriangles();

	// allocate and clear table
	static RRVec3 pink(1,0,1); // pink = preimport vertices without ivertex
	RR_ASSERT(priv->preVertex2Ivertex.empty()); // full clear to pink. without full clear, invalid values would stay alive in vertices we don't overwrite (e.g. needles)
	priv->preVertex2Ivertex.resize(1+priv->objects.size());
	priv->preVertex2Ivertex[0].resize(numPostImportMultiTriangles*3,&pink);
	for(int objectHandle=0;objectHandle<(int)priv->objects.size();objectHandle++)
	{
		priv->preVertex2Ivertex[1+objectHandle].resize(getIllumination(objectHandle) ? getIllumination(objectHandle)->getNumPreImportVertices() : 0,&pink);
	}
	
	// fill tables
	for(unsigned postImportMultiTriangle=0;postImportMultiTriangle<numPostImportMultiTriangles;postImportMultiTriangle++)
	{
		RRMesh::Triangle postImportMultiTriangleVertices;
		multiMesh->getTriangle(postImportMultiTriangle,postImportMultiTriangleVertices);
		for(unsigned v=0;v<3;v++)
		{
			// fir multiobject
			priv->preVertex2Ivertex[0][postImportMultiTriangle*3+v] = priv->packedSolver->getTriangleIrradianceIndirect(postImportMultiTriangle,v);
			// for singleobjects
			unsigned postImportMultiVertex = postImportMultiTriangleVertices[v];
			if(postImportMultiVertex<numPostImportMultiVertices)
			{
				const RRVec3* irrad = priv->packedSolver->getTriangleIrradianceIndirect(postImportMultiTriangle,v);
				if(irrad)
				{
					RRMesh::PreImportNumber preVertexMulti = multiMesh->getPreImportVertex(postImportMultiVertex,postImportMultiTriangle);
					priv->preVertex2Ivertex[1+preVertexMulti.object][preVertexMulti.index] = irrad;
				}
			}
			else
			{
				// should not get here. let it pink
				RR_ASSERT(0);
			}
		}
	}
}

//! Updates vertex buffer with direct, indirect or global illumination on single static object's surface.
//
//! \param objectNumber
//!  Number of object in this scene.
//!  Object numbers are defined by order in which you pass objects to setStaticObjects().
//!  -1 for multiobject with whole static scene.
//! \param vertexBuffer
//!  Destination vertex buffer for indirect illumination.
//! \param params
//!  Parameters of the update process, NULL for the default parameters that
//!  specify fast update (takes milliseconds) of RM_IRRADIANCE_PHYSICAL_INDIRECT data.
//!  \n Only subset of all parameters is supported, see remarks.
//!  \n params->measure specifies type of information stored in vertex buffer.
//!  For typical scenario with per pixel direct illumination and per vertex indirect illumination,
//!  use RM_IRRADIANCE_PHYSICAL_INDIRECT (faster) or RM_IRRADIANCE_CUSTOM_INDIRECT.
//! \return
//!  Number of vertex buffers updated, 0 or 1.
//! \remarks
//!  In comparison with more general updateLightmaps() function, this one
//!  lacks paramsIndirect. However, you can still include indirect illumination
//!  while updating single vertex buffer, see updateLightmaps() remarks.
//! \remarks
//!  In comparison with updateLightmap(),
//!  updateVertexBufferFromSolver() is very fast but less general, it always reads lighting from current solver,
//!  without final gather. In other words, it assumes that
//!  params.applyCurrentSolution=1; applyLights=0; applyEnvironment=0.
//!  For higher quality final gathered results, use updateLightmaps().
unsigned RRDynamicSolver::updateVertexBufferFromSolver(int objectNumber, RRBuffer* vertexBuffer, const UpdateParameters* params)
{
	RRReporter::report(INF3,"Updating vertex buffer for object %d/%d.\n",objectNumber,getNumObjects());

	if(!vertexBuffer || objectNumber>=(int)getNumObjects() || objectNumber<-1)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned numPreImportVertices = (objectNumber>=0)
		? getIllumination(objectNumber)->getNumPreImportVertices() // elements in original object vertex buffer
		: getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()*3; // elements in multiobject vertex buffer
	if(vertexBuffer->getType()!=BT_VERTEX_BUFFER || vertexBuffer->getWidth()<numPreImportVertices)
	{
		RR_ASSERT(0);
		return 0;
	}

	// packed solver
	if(priv->packedSolver)
	{
		RR_ASSERT(priv->preVertex2Ivertex.size()==1+getNumObjects());
		priv->packedSolver->getTriangleIrradianceIndirectUpdate();
		const std::vector<const RRVec3*>& preVertex2Ivertex = priv->preVertex2Ivertex[1+objectNumber];
		RR_ASSERT(preVertex2Ivertex.size()==numPreImportVertices);
		RRVec3* lock = vertexBuffer->getFormat()==BF_RGBF ? (RRVec3*)(vertexBuffer->lock(BL_DISCARD_AND_WRITE)) : NULL;
		if(lock)
		{
			// #pragma with if() is broken in VC++2005
			if(numPreImportVertices>35000)
			{
				#pragma omp parallel for schedule(static)
				for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
				{
					lock[preImportVertex] = *preVertex2Ivertex[preImportVertex];
				}
			}
			else
			{
				for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
				{
					lock[preImportVertex] = *preVertex2Ivertex[preImportVertex];
				}
			}
			vertexBuffer->unlock();
		}
		else
		{
			for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
			{
				RRVec4 tmp;
				tmp = *preVertex2Ivertex[preImportVertex];
				vertexBuffer->setElement(preImportVertex,tmp);
			}
		}
		return 1;
	}

	// dynamic solver
	if(!priv->scene)
	{
		calculateCore(0); // create missing solver
		if(!priv->scene)
		{
			RRReporter::report(WARN,"Empty scene, vertex buffer not set.\n");
			RR_ASSERT(0);
			return 0;
		}
	}
	RRRadiometricMeasure measure = params ? params->measure_internal : RM_IRRADIANCE_CUSTOM_INDIRECT;
	measure.scaled = vertexBuffer->getScaled();

	// load measure into each preImportVertex
#pragma omp parallel for schedule(static)
	for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
	{
		unsigned t = (objectNumber<0)?preImportVertex/3:priv->preVertex2PostTriangleVertex[objectNumber][preImportVertex].triangleIndex;
		unsigned v = (objectNumber<0)?preImportVertex%3:priv->preVertex2PostTriangleVertex[objectNumber][preImportVertex].vertex012;
		RRVec4 indirect = RRVec4(0);
		if(t<0x3fffffff) // UNDEFINED clamped to 30bit
		{
			priv->scene->getTriangleMeasure(t,v,measure,priv->scaler,indirect);
			// make it optional when negative values are supported
			//for(unsigned i=0;i<3;i++)
			//	indirect[i] = MAX(0,indirect[i]);
			for(unsigned i=0;i<3;i++)
			{
				RR_ASSERT(_finite(indirect[i]));
				RR_ASSERT(indirect[i]<1500000);
			}
		}
		vertexBuffer->setElement(preImportVertex,indirect);
	}
	return 1;
}

// Converts data from input array [post import triangles of whole scene]
// to output array [pre import vertices of single object]
// Fast, but used only in offline solutions.
unsigned RRDynamicSolver::updateVertexBufferFromPerTriangleData(unsigned objectHandle, RRBuffer* vertexBuffer, RRVec3* perTriangleData, unsigned stride, bool allowScaling) const
{
	RRReporter::report(INF3,"Updating object %d/%d, vertex buffer.\n",objectHandle,getNumObjects());

	if(!priv->scene || !vertexBuffer || !getIllumination(objectHandle))
	{
		RR_ASSERT(0);
		return 0;
	}
	const RRScaler* scaler = (vertexBuffer->getScaled() && allowScaling) ? priv->scaler : NULL;
	unsigned numPreImportVertices = getIllumination(objectHandle)->getNumPreImportVertices();
	// load measure into each preImportVertex
#pragma omp parallel for schedule(static)
	for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
	{
		unsigned t = priv->preVertex2PostTriangleVertex[objectHandle][preImportVertex].triangleIndex;
		unsigned v = priv->preVertex2PostTriangleVertex[objectHandle][preImportVertex].vertex012;
		RRVec4 data = RRVec4(0);
		if(t<0x3fffffff) // UNDEFINED clamped to 30bit
		{
			data = priv->scene->getVertexDataFromTriangleData(t,v,perTriangleData,stride);
			if(scaler) scaler->getCustomScale(data);
			for(unsigned i=0;i<3;i++)
			{
				RR_ASSERT(_finite(data[i]));
				RR_ASSERT(data[i]<1500000);
			}
		}
		vertexBuffer->setElement(preImportVertex,data);
	}
	return 1;
}

} // namespace
