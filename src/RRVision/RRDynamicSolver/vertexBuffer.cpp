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

unsigned RRDynamicSolver::updateVertexBuffer(unsigned objectHandle, RRIlluminationVertexBuffer* vertexBuffer, RRRadiometricMeasure measure)
{
	if(!scene || !vertexBuffer)
	{
		RR_ASSERT(0);
		return 0;
	}
	RRObjectIllumination* illumination = getIllumination(objectHandle);
	unsigned numPreImportVertices = illumination->getNumPreImportVertices();
	// load measure into each preImportVertex
#pragma omp parallel for schedule(static,1)
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

unsigned RRDynamicSolver::updateVertexBuffers(unsigned channelNumber, bool createMissingBuffers, RRRadiometricMeasure measure)
{
	unsigned updatedBuffers = 0;
	if(!scene)
	{
		RR_ASSERT(0);
		return updatedBuffers;
	}
	REPORT_INIT;
	REPORT_BEGIN("Updating vertex buffers.");
	// for each object
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();
		RRObjectIllumination::Channel* channel = illumination->getChannel(channelNumber);
		if(!channel->vertexBuffer && createMissingBuffers) channel->vertexBuffer = newVertexBuffer(numPreImportVertices);
		if(channel->vertexBuffer)
		{
			updatedBuffers += updateVertexBuffer(objectHandle,channel->vertexBuffer,measure);
		}
	}
	REPORT_END;
	return updatedBuffers;
}

} // namespace
