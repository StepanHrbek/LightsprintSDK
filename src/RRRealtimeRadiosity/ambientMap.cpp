#define MULTIOBJECT // creates multiObject to accelerate calculation

#include <assert.h>
#include "RRRealtimeRadiosity.h"

namespace rr
{

struct RenderSubtriangleContext
{
	RRIlluminationPixelBuffer* pixelBuffer;
	RRObject::TriangleMapping triangleMapping;
};

void renderSubtriangle(const RRScene::SubtriangleIllumination& si, void* context)
{
	RenderSubtriangleContext* context2 = (RenderSubtriangleContext*)context;
	RRIlluminationPixelBuffer::IlluminatedTriangle si2;
	for(unsigned i=0;i<3;i++)
	{
		si2.iv[i].measure = si.measure[i];
		assert(context2->triangleMapping.uv[0][0]>=0 && context2->triangleMapping.uv[0][0]<=1);
		assert(context2->triangleMapping.uv[0][1]>=0 && context2->triangleMapping.uv[0][1]<=1);
		assert(context2->triangleMapping.uv[1][0]>=0 && context2->triangleMapping.uv[1][0]<=1);
		assert(context2->triangleMapping.uv[1][1]>=0 && context2->triangleMapping.uv[1][1]<=1);
		assert(context2->triangleMapping.uv[2][0]>=0 && context2->triangleMapping.uv[2][0]<=1);
		assert(context2->triangleMapping.uv[2][1]>=0 && context2->triangleMapping.uv[2][1]<=1);
		assert(si.texCoord[i][0]>=0 && si.texCoord[i][0]<=1);
		assert(si.texCoord[i][1]>=0 && si.texCoord[i][1]<=1);
		// si.texCoord 0,0 prevest na context2->triangleMapping.uv[0]
		// si.texCoord 1,0 prevest na context2->triangleMapping.uv[1]
		// si.texCoord 0,1 prevest na context2->triangleMapping.uv[2]
		si2.iv[i].texCoord = context2->triangleMapping.uv[0] + (context2->triangleMapping.uv[1]-context2->triangleMapping.uv[0])*si.texCoord[i][0] + (context2->triangleMapping.uv[2]-context2->triangleMapping.uv[0])*si.texCoord[i][1];
		assert(si2.iv[i].texCoord[0]>=0 && si2.iv[i].texCoord[0]<=1);
		assert(si2.iv[i].texCoord[1]>=0 && si2.iv[i].texCoord[1]<=1);
		for(unsigned j=0;j<3;j++)
		{
			assert(_finite(si2.iv[i].measure[j]));
			assert(si2.iv[i].measure[j]>=0);
			assert(si2.iv[i].measure[j]<1500000);
		}
	}
	context2->pixelBuffer->renderTriangle(si2);
}

RRIlluminationPixelBuffer* RRRealtimeRadiosity::newPixelBuffer(RRObject* object)
{
	return NULL;
}

void RRRealtimeRadiosity::readPixelResults()
{
	if(!scene)
	{
		assert(0);
		return;
	}
	// for each object
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
#ifdef MULTIOBJECT
		RRObject* object = getMultiObject();
#else
		RRObject* object = getObject(objectHandle);
#endif
		if(!object)
		{
			assert(0);
			return;
		}
		RRMesh* mesh = object->getCollider()->getMesh();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
		if(!channel->pixelBuffer) channel->pixelBuffer = newPixelBuffer(object);
		RRIlluminationPixelBuffer* pixelBuffer = channel->pixelBuffer;

		if(pixelBuffer)
		{
			pixelBuffer->renderBegin();

			// for each triangle
			for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
			{
				// render all subtriangles into pixelBuffer using object's unwrap
				RenderSubtriangleContext rsc;
				rsc.pixelBuffer = pixelBuffer;
				object->getTriangleMapping(postImportTriangle,rsc.triangleMapping);
#ifdef MULTIOBJECT
				// multiObject must preserve mapping (all objects overlap in one map)
				//!!! this is satisfied now, but it may change in future
				RRMesh::MultiMeshPreImportNumber preImportTriangle = mesh->getPreImportTriangle(postImportTriangle);
				if(preImportTriangle.object==objectHandle)
				{
					scene->getSubtriangleMeasure(0,postImportTriangle,RM_IRRADIANCE_SCALED_INDIRECT,renderSubtriangle,&rsc);
				}
#else
				scene->getSubtriangleMeasure(objectHandle,postImportTriangle,RM_IRRADIANCE_SCALED_INDIRECT,renderSubtriangle,&rsc)
#endif
			}
			pixelBuffer->renderEnd();
		}
	}
}

} // namespace
