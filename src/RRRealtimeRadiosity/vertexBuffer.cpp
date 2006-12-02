#define MULTIOBJECT // creates multiObject to accelerate calculation

#include <assert.h>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "RRRealtimeRadiosity.h"

namespace rr
{

void RRRealtimeRadiosity::updateVertexLookupTable()
// prepare lookup tables preImportVertex -> [postImportTriangle,vertex0..2] for all objects
{
	if(!getMultiObject())
	{
		assert(0);
		return;
	}
	preVertex2PostTriangleVertex.resize(objects.size());
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		RRMesh* mesh =
#ifdef MULTIOBJECT
			getMultiObject()->getCollider()->getMesh();
#else
			object->getCollider()->getMesh();
#endif
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
#ifdef MULTIOBJECT
					RRMesh::MultiMeshPreImportNumber preVertexMulti = preVertex;
					if(preVertexMulti.object==objectHandle)
						preVertex = preVertexMulti.index;
					else
						continue; // skip asserts
#endif
					if(preVertex<numPreImportVertices)
						preVertex2PostTriangleVertex[objectHandle][preVertex] = std::pair<unsigned,unsigned>(postImportTriangle,v);
					else
						assert(0);
				}
				else
					assert(0);
			}
		}
	}
}

RRIlluminationVertexBuffer* RRRealtimeRadiosity::newVertexBuffer(unsigned numVertices)
{
	return RRIlluminationVertexBuffer::createInSystemMemory(numVertices);
}

void RRRealtimeRadiosity::readVertexResults()
{
	if(!scene)
	{
		assert(0);
		return;
	}
	// for each object
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();
		RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
		if(!channel->vertexBuffer) channel->vertexBuffer = newVertexBuffer(numPreImportVertices);
		RRIlluminationVertexBuffer* vertexBuffer = channel->vertexBuffer;

		// load measure into each preImportVertex
#pragma omp parallel for schedule(static,1)
		for(int preImportVertex=0;(unsigned)preImportVertex<numPreImportVertices;preImportVertex++)
		{
			unsigned t = preVertex2PostTriangleVertex[objectHandle][preImportVertex].first;
			unsigned v = preVertex2PostTriangleVertex[objectHandle][preImportVertex].second;
			RRColor indirect = RRColor(0);
			if(t!=RRMesh::UNDEFINED && v!=RRMesh::UNDEFINED)
			{
#ifdef MULTIOBJECT
				scene->getTriangleMeasure(0,t,v,RM_IRRADIANCE_SCALED_INDIRECT,indirect);
#else
				scene->getTriangleMeasure(objectHandle,t,v,RM_IRRADIANCE_SCALED_INDIRECT,indirect);
#endif
				for(unsigned i=0;i<3;i++)
				{
					assert(_finite(indirect[i]));
					assert(indirect[i]>=0);
					assert(indirect[i]<1500000);
				}
			}
			vertexBuffer->setVertex(preImportVertex,indirect);
		}
	}
}

} // namespace
