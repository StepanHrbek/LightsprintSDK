#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "Lightsprint/RRDynamicSolver.h"
#include "report.h"
#include "private.h"
#include "gather.h"

namespace rr
{

void RRDynamicSolver::updateVertexLookupTableDynamicSolver()
// prepare lookup tables preImportVertex -> [postImportTriangle,vertex012] for all objects
{
	if(!getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	priv->preVertex2PostTriangleVertex.resize(priv->objects.size());
	for(unsigned objectHandle=0;objectHandle<priv->objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		RRMesh* mesh = getMultiObjectPhysical()->getCollider()->getMesh();
		unsigned numPostImportVertices = mesh->getNumVertices();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();

		priv->preVertex2PostTriangleVertex[objectHandle].resize(numPreImportVertices,Private::TriangleVertexPair(RRMesh::UNDEFINED,RRMesh::UNDEFINED));

		for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
		{
			RRMesh::Triangle postImportTriangleVertices;
			mesh->getTriangle(postImportTriangle,postImportTriangleVertices);
			for(unsigned v=0;v<3;v++)
			{
				unsigned postImportVertex = postImportTriangleVertices[v];
				if(postImportVertex<numPostImportVertices)
				{
					unsigned preVertex = mesh->getPreImportVertex(postImportVertex,postImportTriangle);
					RRMesh::MultiMeshPreImportNumber preVertexMulti = preVertex;
					if(preVertexMulti.object==objectHandle)
						preVertex = preVertexMulti.index;
					else
						continue; // skip asserts
					if(preVertex<numPreImportVertices)
						priv->preVertex2PostTriangleVertex[objectHandle][preVertex] = Private::TriangleVertexPair(postImportTriangle,v);
					else
						RR_ASSERT(0);
				}
				else
					RR_ASSERT(0);
			}
		}
	}
}

void RRDynamicSolver::updateVertexLookupTablePackedSolver()
// prepare lookup tables preImportVertex -> Ivertex for multiobject and for all singleobjects
{
	if(!priv->packedSolver || !getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	priv->preVertex2Ivertex.resize(1+priv->objects.size());
	for(int objectHandle=-1;objectHandle<(int)priv->objects.size();objectHandle++)
	{
		RRMesh* mesh = getMultiObjectPhysical()->getCollider()->getMesh();
		unsigned numPostImportVertices = mesh->getNumVertices();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		unsigned numPreImportVertices = (objectHandle<0) // elements in vertex buffer
			? numPostImportTriangles*3 // num post import triangles * 3
			: getIllumination(objectHandle)->getNumPreImportVertices(); // num pre import vertices

		priv->preVertex2Ivertex[1+objectHandle].resize(numPreImportVertices,NULL);

		for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
		{
			RRMesh::Triangle postImportTriangleVertices;
			mesh->getTriangle(postImportTriangle,postImportTriangleVertices);
			for(unsigned v=0;v<3;v++)
			{
				if(objectHandle<0)
				{
					// for multiobject
					priv->preVertex2Ivertex[0][postImportTriangle*3+v] = priv->packedSolver->getTriangleIrradianceIndirect(postImportTriangle,v);
				}
				else
				{
					// for singleobjects
					unsigned postImportVertex = postImportTriangleVertices[v];
					if(postImportVertex<numPostImportVertices)
					{
						unsigned preVertex = mesh->getPreImportVertex(postImportVertex,postImportTriangle);
						RRMesh::MultiMeshPreImportNumber preVertexMulti = preVertex;
						if(preVertexMulti.object==objectHandle)
							preVertex = preVertexMulti.index;
						else
							continue; // skip asserts
						if(preVertex<numPreImportVertices)
						{
							/* if more ivertices writes to this vertex, stop after getting first valid pointer and then never overwrite it by NULL
							proc muze vic ivertexu zapisovat do stejneho vertexu a jeden z nich je NULL?
							builder:
								mam triangl, neni uplny degen, ale je to jehlicka
								nekdo mu proto da surface=null
								cele tri ivertexy jsou null
								vezmeme jeden z vrcholu
								ivertex je null
								vertex je sdilen vice triangly, takze na stejne pozici je i pekny ivertex obsahujici okolni pekne triangly
								oba ivertexy zapisu do fib
							runtime:
								ve scene je jen 1 vertex
								kdyz sestavuju vbuf, pro tento 1 vertex hledam ivertex
								najdu oba
								musim z nich pouzit ten ktery neni fialovy*/
							if(!priv->preVertex2Ivertex[1+objectHandle][preVertex])
								priv->preVertex2Ivertex[1+objectHandle][preVertex] = priv->packedSolver->getTriangleIrradianceIndirect(postImportTriangle,v);
						}
						else
							RR_ASSERT(0);
					}
					else
						RR_ASSERT(0);
				}
			}
		}

		// pink = preimport vertices without ivertex
		for(unsigned preImportVertex=0;preImportVertex<numPreImportVertices;preImportVertex++)
		{
			static RRVec3 pink(1,0,1);
			if(!priv->preVertex2Ivertex[1+objectHandle][preImportVertex])
				priv->preVertex2Ivertex[1+objectHandle][preImportVertex] = &pink;
		}
	}
}

RRIlluminationVertexBuffer* RRDynamicSolver::newVertexBuffer(unsigned numVertices)
{
	return RRIlluminationVertexBuffer::createInSystemMemory(numVertices);
}

unsigned RRDynamicSolver::updateVertexBuffer(int objectHandle, RRIlluminationVertexBuffer* vertexBuffer, const UpdateParameters* params)
{
	if(!vertexBuffer || objectHandle>=(int)getNumObjects() || objectHandle<-1)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned numPreImportVertices = (objectHandle>=0)
		? getIllumination(objectHandle)->getNumPreImportVertices() // elements in original object vertex buffer
		: getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()*3; // elements in multiobject vertex buffer

	// packed solver
	if(priv->packedSolver)
	{
		priv->packedSolver->getTriangleIrradianceIndirectUpdate();
		const std::vector<const RRVec3*>& preVertex2Ivertex = priv->preVertex2Ivertex[1+objectHandle];
		RRColorRGBF* lock = vertexBuffer->lockWriting();
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
				vertexBuffer->setVertex(preImportVertex,*preVertex2Ivertex[preImportVertex]);
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
	RRRadiometricMeasure measure = params ? params->measure : RM_IRRADIANCE_PHYSICAL_INDIRECT;
	// load measure into each preImportVertex
#pragma omp parallel for schedule(static)
	for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
	{
		unsigned t = (objectHandle<0)?preImportVertex/3:priv->preVertex2PostTriangleVertex[objectHandle][preImportVertex].triangleIndex;
		unsigned v = (objectHandle<0)?preImportVertex%3:priv->preVertex2PostTriangleVertex[objectHandle][preImportVertex].vertex012;
		RRColor indirect = RRColor(0);
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
		vertexBuffer->setVertex(preImportVertex,indirect);
	}
	return 1;
}

// post import triangly cele sceny -> pre import vertexy jednoho objektu
unsigned RRDynamicSolver::updateVertexBufferFromPerTriangleData(unsigned objectHandle, RRIlluminationVertexBuffer* vertexBuffer, RRVec3* perTriangleData, unsigned stride) const
{
	if(!priv->scene || !vertexBuffer)
	{
		RR_ASSERT(0);
		return 0;
	}
	unsigned numPreImportVertices = getIllumination(objectHandle)->getNumPreImportVertices();
	// load measure into each preImportVertex
#pragma omp parallel for schedule(static)
	for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
	{
		unsigned t = priv->preVertex2PostTriangleVertex[objectHandle][preImportVertex].triangleIndex;
		unsigned v = priv->preVertex2PostTriangleVertex[objectHandle][preImportVertex].vertex012;
		RRVec3 data = RRVec3(0);
		if(t!=RRMesh::UNDEFINED && v!=RRMesh::UNDEFINED)
		{
			data = getStaticSolver()->getVertexDataFromTriangleData(t,v,perTriangleData,stride);
			for(unsigned i=0;i<3;i++)
			{
				RR_ASSERT(_finite(data[i]));
				RR_ASSERT(data[i]<1500000);
			}
		}
		vertexBuffer->setVertex(preImportVertex,data);
	}
	return 1;
}

unsigned RRDynamicSolver::updateVertexBuffers(int layerNumberLighting, int layerNumberBentNormals, bool createMissingBuffers, const UpdateParameters* aparamsDirect, const UpdateParameters* aparamsIndirect)
{
	UpdateParameters paramsDirect;
	paramsDirect.applyCurrentSolution = true; // NULL = realtime update, bez final gatheru
	paramsDirect.applyEnvironment = false;
	paramsDirect.applyLights = false;
	UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = false;
	paramsIndirect.applyLights = false;
	if(aparamsDirect) paramsDirect = *aparamsDirect;
	if(aparamsIndirect) paramsIndirect = *aparamsIndirect;

	RRReportInterval report((aparamsDirect || aparamsIndirect)?INF1:INF3,"Updating vertex buffers (%d,%d,DIRECT(%s%s%s%s%s),INDIRECT(%s%s%s%s%s)).\n",
		layerNumberLighting,layerNumberBentNormals,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",
		(paramsDirect.applyCurrentSolution&&paramsDirect.measure.direct)?"D":"",
		(paramsDirect.applyCurrentSolution&&paramsDirect.measure.indirect)?"I":"",
		paramsDirect.applyCurrentSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.direct)?"D":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.indirect)?"I":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");

	// 0. create missing buffers
	if(createMissingBuffers)
	{
		for(unsigned object=0;object<getNumObjects();object++)
		{
			if(layerNumberLighting>=0)
			{
				RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumberLighting);
				if(layer && !layer->vertexBuffer) layer->vertexBuffer = newVertexBuffer(getObject(object)->getCollider()->getMesh()->getNumVertices());
			}
			if(layerNumberBentNormals>=0)
			{
				RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumberBentNormals);
				if(layer && !layer->vertexBuffer) layer->vertexBuffer = newVertexBuffer(getObject(object)->getCollider()->getMesh()->getNumVertices());
			}
		}
	}

	if(paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		if(aparamsDirect) // don't report if direct is NULL, silently disable it
			RRReporter::report(WARN,"paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
		paramsDirect.applyCurrentSolution = false;
	}

	// 1. first gather: solver+lights+env -> solver.direct
	// 2. propagate: solver.direct -> solver.indirect
	if(aparamsIndirect && (paramsIndirect.applyCurrentSolution || paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		// auto quality for first gather
		// shoot 4x less indirect rays than direct
		// (but only if direct.quality was specified)
		if(aparamsDirect)
			paramsIndirect.quality = paramsDirect.quality/4;

		if(!updateSolverIndirectIllumination(&paramsIndirect,
				getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles(),
				paramsDirect.quality)
			)
			return 0;

		paramsDirect.applyCurrentSolution = true; // set solution generated here to be gathered in final gather
		paramsDirect.measure.direct = true; // it is stored in direct+indirect (after calculate)
		paramsDirect.measure.indirect = true;
		// musim gathernout direct i indirect.
		// indirect je jasny, jedine v nem je vysledek spocteny v calculate().
		// ovsem direct musim tez, jedine z nej dostanu prime osvetleni emisivnim facem, prvni odraz od spotlight, lights a env
	}

	unsigned updatedBuffers = 0;

	if(paramsDirect.applyLights || paramsDirect.applyEnvironment || (paramsDirect.applyCurrentSolution && paramsDirect.quality))
	{
		// non realtime update

		// 3. final gather: solver.direct+indirect+lights+env -> tmparray
		unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
		ProcessTexelResult* finalGather = new ProcessTexelResult[numTriangles];
		gatherPerTriangle(&paramsDirect,finalGather,numTriangles);

		// 4. interpolate: tmparray -> buffer
		// for each object
		for(unsigned objectHandle=0;objectHandle<priv->objects.size();objectHandle++)
		{
			if(layerNumberLighting>=0)
			{
				RRIlluminationVertexBuffer* vertexColors = getIllumination(objectHandle)->getLayer(layerNumberLighting)->vertexBuffer;
				updatedBuffers += updateVertexBufferFromPerTriangleData(objectHandle,vertexColors,&finalGather[0].irradiance,sizeof(finalGather[0]));
			}
			if(layerNumberBentNormals>=0)
			{
				RRIlluminationVertexBuffer* bentNormals = getIllumination(objectHandle)->getLayer(layerNumberBentNormals)->vertexBuffer;
				updatedBuffers += updateVertexBufferFromPerTriangleData(objectHandle,bentNormals,&finalGather[0].bentNormal,sizeof(finalGather[0]));
			}
		}
		delete[] finalGather;
	}
	else
	if(paramsDirect.applyCurrentSolution)
	{
		// realtime update

		for(int objectHandle=0;objectHandle<(int)priv->objects.size();objectHandle++)
		{
			if(layerNumberLighting>=0)
			{
				RRIlluminationVertexBuffer* vertexColors = getIllumination(objectHandle)->getLayer(layerNumberLighting)->vertexBuffer;
				if(vertexColors)
					updatedBuffers += updateVertexBuffer(objectHandle,vertexColors,&paramsDirect);
			}
			if(layerNumberBentNormals>=0)
			{
				RRIlluminationVertexBuffer* bentNormals = getIllumination(objectHandle)->getLayer(layerNumberBentNormals)->vertexBuffer;
				if(bentNormals)
					RRReporter::report(WARN,"Bent normals not supported in 'realtime' mode (quality=0).\n");
			}
		}
	}
	else
	{
		RRReporter::report(WARN,"No light sources enabled.\n");
		RR_ASSERT(0);
	}

	return updatedBuffers;
}

} // namespace
