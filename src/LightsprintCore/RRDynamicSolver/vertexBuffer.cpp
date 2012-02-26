// --------------------------------------------------------------------------
// Offline and realtime per-vertex lightmaps.
// Copyright (c) 2006-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

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
// prepare lookup tables postImportVertex -> [postImportTriangle,vertex012] for all objects
// depends on static objects (needs update when they change)
{
	// table depends only on static scene
	// setStaticObjects() clears it, so if it's not empty, it's already up to date
	if (!priv->postVertex2PostTriangleVertex.empty()) return;

	RRReportInterval reportProp(INF3,"Updating vertex lookup table...\n");
	if (!getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	// allocate and clear table
	// (_full_ clear to UNDEFINED. without full clear, invalid values would stay alive in vertices we don't overwrite (e.g. needles))
	priv->postVertex2PostTriangleVertex.resize(getStaticObjects().size());
	for (unsigned objectHandle=0;objectHandle<getStaticObjects().size();objectHandle++)
	{
		RRObject* object = getStaticObjects()[objectHandle];
		unsigned numPostImportSingleVertices = object->getCollider()->getMesh()->getNumVertices();
		priv->postVertex2PostTriangleVertex[objectHandle].resize(numPostImportSingleVertices,Private::TriangleVertexPair(RRMesh::UNDEFINED,RRMesh::UNDEFINED));
	}
	// fill table
	const RRMesh* multiMesh = getMultiObjectPhysical()->getCollider()->getMesh();
	unsigned numPostImportMultiVertices = multiMesh->getNumVertices();
	unsigned numPostImportMultiTriangles = multiMesh->getNumTriangles();
	for (unsigned postImportMultiTriangle=0;postImportMultiTriangle<numPostImportMultiTriangles;postImportMultiTriangle++)
	{
		RRMesh::Triangle postImportMultiTriangleVertices;
		multiMesh->getTriangle(postImportMultiTriangle,postImportMultiTriangleVertices);
		for (unsigned v=0;v<3;v++)
		{
			unsigned postImportMultiVertex = postImportMultiTriangleVertices[v];
			if (postImportMultiVertex<numPostImportMultiVertices)
			{
				RRMesh::PreImportNumber postVertexMulti =
					//!!! here we calculate preimport data
					//    for postimport 1obj data, multiobj would have to remove preimport numbering
					multiMesh->getPreImportVertex(postImportMultiVertex,postImportMultiTriangle);
				if (priv->scene && !priv->scene->scene->object->triangle[postImportMultiTriangle].topivertex[v])
				{
					// static solver doesn't like this triangle and set surface NULL, probably because it is a needle
					// let it UNDEFINED
				}
				else
				{
					priv->postVertex2PostTriangleVertex[postVertexMulti.object][postVertexMulti.index] = Private::TriangleVertexPair(postImportMultiTriangle,v);
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
// depends on static objects and packedSolver (needs update when they change)
{
	RRReportInterval reportProp(INF3,"Updating fireball lookup table...\n");
	if (!priv->packedSolver || !getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	const RRMesh* multiMesh = getMultiObjectPhysical()->getCollider()->getMesh();
	unsigned numPostImportMultiVertices = multiMesh->getNumVertices();
	unsigned numPostImportMultiTriangles = multiMesh->getNumTriangles();

	// allocate and clear table
#ifdef _DEBUG
	static RRVec3 pink(1,0,1); // pink = preimport vertices without ivertex
#else
	static RRVec3 pink(0);
#endif
	RR_ASSERT(priv->postVertex2Ivertex.empty()); // _full_ clear to pink. without full clear, invalid values would stay alive in vertices we don't overwrite (e.g. needles)
	priv->postVertex2Ivertex.resize(1+getStaticObjects().size());
	priv->postVertex2Ivertex[0].resize(numPostImportMultiVertices,&pink); // [multiobj indir is indexed]
	for (int objectHandle=0;objectHandle<(int)getStaticObjects().size();objectHandle++)
	{
		priv->postVertex2Ivertex[1+objectHandle].resize(getStaticObjects()[objectHandle]->getCollider()->getMesh()->getNumVertices(),&pink);
	}
	
	// fill tables
	for (unsigned postImportMultiTriangle=0;postImportMultiTriangle<numPostImportMultiTriangles;postImportMultiTriangle++)
	{
		RRMesh::Triangle postImportMultiTriangleVertices;
		multiMesh->getTriangle(postImportMultiTriangle,postImportMultiTriangleVertices);
		for (unsigned v=0;v<3;v++)
		{
			unsigned postImportMultiVertex = postImportMultiTriangleVertices[v];
			if (postImportMultiVertex<numPostImportMultiVertices)
			{
				const RRVec3* irrad = priv->packedSolver->getTriangleIrradianceIndirect(postImportMultiTriangle,v);
				if (irrad)
				{
					// for multiobject
					priv->postVertex2Ivertex[0][postImportMultiVertex] = irrad; // [multiobj indir is indexed]
					// for singleobjects
					RRMesh::PreImportNumber postVertexMulti =
						//!!! here we calculate preimport data
						//    for postimport 1obj data, multiobj has to remove preimport numbering from 1objs
						multiMesh->getPreImportVertex(postImportMultiVertex,postImportMultiTriangle);
					priv->postVertex2Ivertex[1+postVertexMulti.object][postVertexMulti.index] = irrad;
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
	if (objectNumber<10 || (objectNumber<100 && !(objectNumber%10)) || (objectNumber<1000 && !(objectNumber%100)) || !(objectNumber%1000))
		RRReporter::report(INF3,"Updating vertex buffer for object %d/%d.\n",objectNumber,getStaticObjects().size());

	if (!vertexBuffer || objectNumber>=(int)getStaticObjects().size() || objectNumber<-1)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned numPostImportVertices = (objectNumber>=0)
		? getStaticObjects()[objectNumber]->getCollider()->getMesh()->getNumVertices() // elements in 1object vertex buffer
		: getMultiObjectCustom()->getCollider()->getMesh()->getNumVertices(); // elements in multiobject vertex buffer [multiobj indir is indexed]
	if (vertexBuffer->getType()!=BT_VERTEX_BUFFER || vertexBuffer->getWidth()<numPostImportVertices)
	{
		RR_ASSERT(0);
		return 0;
	}

	// packed solver
	if (priv->packedSolver)
	{
		RR_ASSERT(priv->postVertex2Ivertex.size()==1+getStaticObjects().size());
		priv->packedSolver->getTriangleIrradianceIndirectUpdate();
		const std::vector<const RRVec3*>& postVertex2Ivertex = priv->postVertex2Ivertex[1+objectNumber];
		RR_ASSERT(postVertex2Ivertex.size()==numPostImportVertices); // [multiobj indir is indexed]
		RRVec3* lock = vertexBuffer->getFormat()==BF_RGBF ? (RRVec3*)(vertexBuffer->lock(BL_DISCARD_AND_WRITE)) : NULL;
		if (lock)
		{
			// #pragma with if () is broken in VC++2005
			if (numPostImportVertices>35000)
			{
				#pragma omp parallel for schedule(static)
				for (int postImportVertex=0;(unsigned)postImportVertex<numPostImportVertices;postImportVertex++)
				{
					lock[postImportVertex] = *postVertex2Ivertex[postImportVertex];
				}
			}
			else
			{
				for (int postImportVertex=0;(unsigned)postImportVertex<numPostImportVertices;postImportVertex++)
				{
					lock[postImportVertex] = *postVertex2Ivertex[postImportVertex];
				}
			}
			vertexBuffer->unlock();
		}
		else
		{
			for (int postImportVertex=0;(unsigned)postImportVertex<numPostImportVertices;postImportVertex++)
			{
				RRVec4 tmp;
				tmp = *postVertex2Ivertex[postImportVertex];
				vertexBuffer->setElement(postImportVertex,tmp);
			}
		}
		vertexBuffer->version = getSolutionVersion();
		return 1;
	}

	// dynamic solver
	if (!priv->scene)
	{
		calculateCore(0); // create missing solver
		if (!priv->scene)
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
	for (int postImportVertex=0;(unsigned)postImportVertex<numPostImportVertices;postImportVertex++)
	{
		unsigned t = (objectNumber<0)?postImportVertex/3:priv->postVertex2PostTriangleVertex[objectNumber][postImportVertex].triangleIndex;
		unsigned v = (objectNumber<0)?postImportVertex%3:priv->postVertex2PostTriangleVertex[objectNumber][postImportVertex].vertex012;
		RRVec4 indirect = RRVec4(0);
		if (t<0x3fffffff) // UNDEFINED clamped to 30bit
		{
			priv->scene->getTriangleMeasure(t,v,measure,priv->scaler,indirect);
			// make it optional when negative values are supported
			//for (unsigned i=0;i<3;i++)
			//	indirect[i] = RR_MAX(0,indirect[i]);
			for (unsigned i=0;i<3;i++)
			{
				RR_ASSERT(_finite(indirect[i]));
				RR_ASSERT(indirect[i]<1500000);
			}
		}
		vertexBuffer->setElement(postImportVertex,indirect);
	}
	vertexBuffer->version = getSolutionVersion();
	return 1;
}

// Converts data from input array [post import triangles of whole scene]
// to output array [pre import vertices of single object]
// Fast, but used only in offline solutions.
unsigned RRDynamicSolver::updateVertexBufferFromPerTriangleDataPhysical(unsigned objectHandle, RRBuffer* vertexBuffer, RRVec3* perTriangleDataPhysical, unsigned stride, bool allowScaling) const
{
	RRReporter::report(INF3,"Updating object %d/%d, vertex buffer.\n",objectHandle,getStaticObjects().size());

	if (!priv->scene || !vertexBuffer || objectHandle>=getStaticObjects().size())
	{
		RR_ASSERT(0);
		return 0;
	}
	const RRScaler* scaler = (vertexBuffer->getScaled() && allowScaling) ? priv->scaler : NULL;
	unsigned numPostImportVertices = getStaticObjects()[objectHandle]->getCollider()->getMesh()->getNumVertices();
	// load measure into each preImportVertex
#pragma omp parallel for schedule(static)
	for (int postImportVertex=0;(unsigned)postImportVertex<numPostImportVertices;postImportVertex++)
	{
		unsigned t = priv->postVertex2PostTriangleVertex[objectHandle][postImportVertex].triangleIndex;
		unsigned v = priv->postVertex2PostTriangleVertex[objectHandle][postImportVertex].vertex012;
		RRVec4 data = RRVec4(0);
		if (t<0x3fffffff) // UNDEFINED clamped to 30bit
		{
			data = priv->scene->getVertexDataFromTriangleData(t,v,perTriangleDataPhysical,stride);
			if (scaler) scaler->getCustomScale(data);
			for (unsigned i=0;i<3;i++)
			{
				RR_ASSERT(_finite(data[i]));
				RR_ASSERT(data[i]<1500000);
			}
		}
		vertexBuffer->setElement(postImportVertex,data);
	}
	vertexBuffer->version = getSolutionVersion();
	return 1;
}

} // namespace
