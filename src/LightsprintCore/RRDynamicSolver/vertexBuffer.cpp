#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "Lightsprint/RRDynamicSolver.h"
#include "report.h"

namespace rr
{

void RRDynamicSolver::updateVertexLookupTable()
// prepare lookup tables preImportVertex -> [postImportTriangle,vertex0..2] for all objects
{
	if(!getMultiObjectPhysical())
	{
		RR_ASSERT(0);
		return;
	}
	preVertex2PostTriangleVertex.resize(objects.size());
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		RRMesh* mesh = getMultiObjectPhysical()->getCollider()->getMesh();
		unsigned numPostImportVertices = mesh->getNumVertices();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();

		preVertex2PostTriangleVertex[objectHandle].resize(numPreImportVertices,std::pair<unsigned,unsigned>(RRMesh::UNDEFINED,RRMesh::UNDEFINED));

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
						preVertex2PostTriangleVertex[objectHandle][preVertex] = std::pair<unsigned,unsigned>(postImportTriangle,v);
					else
						RR_ASSERT(0);
				}
				else
					RR_ASSERT(0);
			}
		}
	}
}

RRIlluminationVertexBuffer* RRDynamicSolver::newVertexBuffer(unsigned numVertices)
{
	return RRIlluminationVertexBuffer::createInSystemMemory(numVertices);
}

unsigned RRDynamicSolver::updateVertexBuffer(unsigned objectHandle, RRIlluminationVertexBuffer* vertexBuffer, const UpdateParameters* params)
{
	if(!scene || !vertexBuffer)
	{
		RR_ASSERT(0);
		return 0;
	}
	RRRadiometricMeasure measure = params ? params->measure : RM_IRRADIANCE_PHYSICAL_INDIRECT;
	RRObjectIllumination* illumination = getIllumination(objectHandle);
	unsigned numPreImportVertices = illumination->getNumPreImportVertices();
	// load measure into each preImportVertex
#pragma omp parallel for schedule(dynamic)
	for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
	{
		unsigned t = preVertex2PostTriangleVertex[objectHandle][preImportVertex].first;
		unsigned v = preVertex2PostTriangleVertex[objectHandle][preImportVertex].second;
		RRColor indirect = RRColor(0);
		if(t!=RRMesh::UNDEFINED && v!=RRMesh::UNDEFINED)
		{
			scene->getTriangleMeasure(t,v,measure,getScaler(),indirect);
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

unsigned RRDynamicSolver::updateVertexBuffers(unsigned layerNumber, bool createMissingBuffers, const UpdateParameters* aparamsDirect, const UpdateParameters* aparamsIndirect)
{
	UpdateParameters paramsDirect;
	paramsDirect.applyCurrentSolution = false;
	paramsDirect.applyEnvironment = false;
	paramsDirect.applyLights = false;
	UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = false;
	paramsIndirect.applyLights = false;
	if(aparamsDirect) paramsDirect = *aparamsDirect;
	if(aparamsIndirect) paramsIndirect = *aparamsIndirect;

	if(aparamsDirect || aparamsIndirect)
	RRReporter::report(RRReporter::INFO,"Updating vertex buffers (%d,DIRECT(%s%s%s%s%s),INDIRECT(%s%s%s%s%s)).\n",
		layerNumber,
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
			RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumber);
			if(!layer->vertexBuffer) layer->vertexBuffer = newVertexBuffer(getObject(object)->getCollider()->getMesh()->getNumVertices());
		}
	}

	if(paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateVertexBuffers: paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
		paramsDirect.applyCurrentSolution = false;
	}

	// 1. first gather into solver.direct
	// 2. propagate
	if(aparamsIndirect)
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
		paramsDirect.measure.direct = false;
		paramsDirect.measure.indirect = true; // it is stored in indirect (after calculate)
	}

	// 3. final gather into solver.direct
	if(paramsDirect.applyLights || paramsDirect.applyEnvironment)
	{
		updateSolverDirectIllumination(&paramsDirect);
		bool tmp = paramsDirect.measure.smoothed;
		paramsDirect.measure = rr::RRRadiometricMeasure(0,0,0,1,0); // it is stored in direct (after reset)
		paramsDirect.measure.smoothed = tmp; // preserve disabled smoothing
	}

	// 4. update buffers from solver.direct
	unsigned updatedBuffers = 0;
	//REPORT_INIT;
	//REPORT_BEGIN("Updating vertex buffers.");
	// for each object
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination::Layer* layer = getIllumination(objectHandle)->getLayer(layerNumber);
		if(layer->vertexBuffer)
		{
			updatedBuffers += updateVertexBuffer(objectHandle,layer->vertexBuffer,&paramsDirect);
		}
	}
	//REPORT_END;
	return updatedBuffers;
}

} // namespace
