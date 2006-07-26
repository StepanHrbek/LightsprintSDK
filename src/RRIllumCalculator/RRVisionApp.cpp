#define MULTIOBJECT // creates multiObject to accelerate calculation

#include <assert.h>
#include <float.h> // _finite
#include <time.h>
#include "RRIllumCalculator.h"

// odsunout do RRIlluminationPixelBuffer.cpp
#include "swraster.h" // RRIlluminationPixelBufferInMemory

namespace rr
{

#define TIME    clock_t
#define GETTIME clock()
#define PER_SEC CLOCKS_PER_SEC

// times in seconds
#define PAUSE_AFTER_CRITICAL_INTERACTION 0.2f // stops calculating after each interaction, improves responsiveness
#define IMPROVE_STEP_NO_INTERACTION 0.1f // length of one improve step when there are no interactions from user
#define IMPROVE_STEP_MIN 0.005f
#define IMPROVE_STEP_MAX 0.08f
#define READING_RESULTS_PERIOD_MIN 0.1f // how often results are readen back. this is increased *1.1 at each read without interaction
#define READING_RESULTS_PERIOD_MAX 1.5f //
// portions in <0..1>
#define MIN_PORTION_SPENT_IMPROVING 0.4f // at least 40% of our time is spent in improving


// odsunout do RRIlluminationPixelBuffer.cpp
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMP(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))
template <class Color>
void RRIlluminationPixelBufferInMemory<Color>::renderTriangle(const SubtriangleIllumination& si)
{
	raster_VERTEX vertex[3];
	raster_POLYGON polygon[3];
	for(unsigned i=0;i<3;i++)
	{
		vertex[i].sx = CLAMP(si.texCoord[i][0],0,1)*width;
		vertex[i].sy = CLAMP(si.texCoord[i][1],0,1)*height;
		vertex[i].u = (si.measure[i][0]+si.measure[i][1]+si.measure[i][2])/3;//!!!
		assert(vertex[i].sx>=0);
		assert(vertex[i].sx<=width);
		assert(vertex[i].sy>=0);
		assert(vertex[i].sy<=height);
	}
	polygon[0].point = &vertex[0];
	polygon[0].next = &polygon[1];
	polygon[1].point = &vertex[1];
	polygon[1].next = &polygon[2];
	polygon[2].point = &vertex[2];
	polygon[2].next = NULL;
	raster_LGouraud(polygon,(raster_COLOR*)pixels,width);
}


RRVisionApp::RRVisionApp()
{
	multiObject = NULL;
	scene = NULL;
	//objects zeroed by constructor
	surfaces = NULL;
	numSurfaces = 0;
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = true;
	lastIlluminationUseTime = 0;
	lastCriticalInteractionTime = 0;
	lastCalcEndTime = 0;
	lastReadingResultsTime = 0;
	userStep = 0;
	calcStep = 0;
	improveStep = 0;
	readingResultsPeriod = 0;
	//preVertex2PostTriangleVertex zeroed by constructor
	resultChannelIndex = 0;
}

RRVisionApp::~RRVisionApp()
{
	delete scene;
	delete surfaces;
}

void RRVisionApp::setObjects(Objects& aobjects)
{
	objects = aobjects;
	dirtyGeometry = true;
}

RRObject* RRVisionApp::getObject(unsigned i)
{
	if(i>=objects.size()) return NULL;
	return objects.at(i).first;
}

RRObjectIlluminationForEditor* RRVisionApp::getIllumination(unsigned i)
{
	if(i>=objects.size()) return NULL;
	return objects.at(i).second;
}

void RRVisionApp::setResultChannel(unsigned channelIndex)
{
	resultChannelIndex = channelIndex;
}

void RRVisionApp::adjustScene()
{
	scene->setScaler(RRScaler::createRgbScaler(0.4f));
}

void RRVisionApp::reportAction(const char* action) const
{
}

void RRVisionApp::reportMaterialChange()
{
	dirtyMaterials = true;
}

void RRVisionApp::reportGeometryChange()
{
	dirtyGeometry = true;
}

void RRVisionApp::reportLightChange()
{
	dirtyLights = true;
}

void RRVisionApp::reportIlluminationUse()
{
	lastIlluminationUseTime = GETTIME;
}

void RRVisionApp::reportCriticalInteraction()
{
	lastCriticalInteractionTime = GETTIME;
}

void RRVisionApp::reportEndOfInteractions()
{
	lastCriticalInteractionTime = 0;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

void RRVisionApp::updateVertexLookupTable()
// prepare lookup tables preImportVertex -> [postImportTriangle,vertex0..2] for all objects
{
	preVertex2PostTriangleVertex.resize(objects.size());
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		RRMesh* mesh =
#ifdef MULTIOBJECT
			multiObject->getCollider()->getMesh();
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

void RRVisionApp::readVertexResults()
{
	// for each object
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();
		RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
		RRIlluminationVertexBuffer* vertexBuffer = channel->vertexBuffer;

		// load measure into each preImportVertex
		for(unsigned preImportVertex=0;preImportVertex<numPreImportVertices;preImportVertex++)
		{
			unsigned t = preVertex2PostTriangleVertex[objectHandle][preImportVertex].first;
			unsigned v = preVertex2PostTriangleVertex[objectHandle][preImportVertex].second;
			RRColor indirect = RRColor(0);
			if(t!=RRMesh::UNDEFINED && v!=RRMesh::UNDEFINED)
			{
#ifdef MULTIOBJECT
				scene->getTriangleMeasure(0,t,v,RM_IRRADIANCE,indirect);
#else
				scene->getTriangleMeasure(objectHandle,t,v,RM_IRRADIANCE,indirect);
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

struct RenderSubtriangleContext
{
	RRIlluminationPixelBuffer* pixelBuffer;
	RRObject::TriangleMapping triangleMapping;
};

void renderSubtriangle(const RRScene::SubtriangleIllumination& si, void* context)
{
	RenderSubtriangleContext* context2 = (RenderSubtriangleContext*)context;
	RRIlluminationPixelBuffer::SubtriangleIllumination si2;
	for(unsigned i=0;i<3;i++)
	{
		si2.measure[i] = si.measure[i];
		// si.texCoord 0,0 prevest na context2->triangleMapping.uv[0]
		// si.texCoord 1,0 prevest na context2->triangleMapping.uv[1]
		// si.texCoord 0,1 prevest na context2->triangleMapping.uv[2]
		si2.texCoord[i] = context2->triangleMapping.uv[0] + context2->triangleMapping.uv[1]*si.texCoord[i][0] + context2->triangleMapping.uv[2]*si.texCoord[i][1];
		for(unsigned j=0;j<3;j++)
		{
			assert(_finite(si2.measure[i][j]));
			assert(si2.measure[i][j]>=0);
			assert(si2.measure[i][j]<1500000);
		}
	}
	context2->pixelBuffer->renderTriangle(si2);
}

void RRVisionApp::readPixelResults()
{
	// for each object
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
#ifdef MULTIOBJECT
		RRObject* object = multiObject;
#else
		RRObject* object = getObject(objectHandle);
#endif
		RRMesh* mesh = object->getCollider()->getMesh();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
		RRIlluminationPixelBuffer* pixelBuffer = channel->pixelBuffer;

		pixelBuffer->markAllUnused();

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
				scene->getSubtriangleMeasure(0,postImportTriangle,RM_IRRADIANCE,renderSubtriangle,&rsc);
			}
#else
			scene->getSubtriangleMeasure(objectHandle,postImportTriangle,RM_IRRADIANCE,renderSubtriangle,&rsc)
#endif
		}
		pixelBuffer->growUsed();
	}
}

#include "../../samples/bunnybenchmark/StopWatch.h"//!!!

RRScene::Improvement RRVisionApp::calculateCore(float improveStep)
{
	bool dirtyFactors = false;
	bool dirtyEnergies = false;
	if(dirtyMaterials)
	{
		dirtyMaterials = false;
		dirtyFactors = true;
		reportAction("Detecting material properties.");
		detectMaterials();
	}
	if(dirtyGeometry)
	{
		dirtyGeometry = false;
		dirtyLights = true;
		dirtyFactors = true;
		if(scene)
		{
			reportAction("Closing old radiosity solver.");
			delete scene;
		}
		reportAction("Opening new radiosity solver.");
		scene = new RRScene;
#ifdef MULTIOBJECT
		RRObject** importers = new RRObject*[objects.size()];
		for(unsigned i=0;i<(unsigned)objects.size();i++)
		{
			importers[i] = objects.at(i).first;
		}
		RRObject* object = RRObject::createMultiObject(importers,(unsigned)objects.size(),RRCollider::IT_BSP_FASTEST,0.01f,true,NULL);
		multiObject = object ? object->createAdditionalIllumination() : NULL;
		delete[] importers;
		scene->objectCreate(multiObject);
#else
		for(Objects::iterator i=objects.begin();i!=objects.end();i++)
			scene->objectCreate((*i).first);
#endif
		adjustScene();
		updateVertexLookupTable();
	}
	if(dirtyLights)
	{
		StopWatch w; w.Start();
		dirtyLights = false;
		dirtyEnergies = true;
		readingResultsPeriod = READING_RESULTS_PERIOD_MIN;
		reportAction("Detecting direct illumination.");
		if(!detectDirectIllumination())
		{
			// detection has failed, ensure these points:
			// 1) detection will be detected again next time
			dirtyLights = true;
			// 2) eventual dirtyFactors = true; won't be forgotten
			// let normal dirtyFactors handler work now, exit later
			// 3) no calculations on current obsoleted primaries will be wasted
			// exit before resetting energies
			// exit before factor calculation and energy propagation
		}
		printf("%d\n",(int)(w.Watch()*1000));
	}
	if(dirtyFactors)
	{
		dirtyFactors = false;
		dirtyEnergies = false;
		reportAction("Resetting solver energies and factors.");
		scene->illuminationReset(true);
	}
	if(dirtyLights)
	{
		// exit in response to unsuccessful detectDirectIllumination
		return RRScene::NOT_IMPROVED;
	}
	if(dirtyEnergies)
	{
		StopWatch w; w.Start();
		dirtyEnergies = false;
		reportAction("Updating solver energies.");
		scene->illuminationReset(false);
		printf("%d\n",(int)(w.Watch()*1000));
	}

	StopWatch w; w.Start();
	reportAction("Calculating.");
	TIME now = GETTIME;
	TIME end = (TIME)(now+improveStep*PER_SEC);
	RRScene::Improvement improvement = scene->illuminationImprove(endByTime,(void*)&end);
	if(improvement==RRScene::FINISHED || improvement==RRScene::INTERNAL_ERROR)
		return improvement;
	printf("  (imp=%f / calc=%f / user=%f)  ",improveStep,calcStep,userStep);
	printf("%d\n",(int)(w.Watch()*1000));

	if(now>=(TIME)(lastReadingResultsTime+readingResultsPeriod*PER_SEC))
	{
		StopWatch w; w.Start();
		reportAction("Reading results.");
		lastReadingResultsTime = now;
		if(readingResultsPeriod<READING_RESULTS_PERIOD_MAX) readingResultsPeriod*=1.1f;
		readVertexResults();
		printf("%d\n",(int)(w.Watch()*1000));
		//readPixelResults();//!!!
		return RRScene::IMPROVED;
	}
	return RRScene::NOT_IMPROVED;
}

RRScene::Improvement RRVisionApp::calculate()
{
	TIME calcBeginTime = GETTIME;
	bool illuminationUse = lastIlluminationUseTime && lastIlluminationUseTime>=lastCalcEndTime;

	// pause during critical interactions
	if(lastCriticalInteractionTime)
	{
		if((calcBeginTime-lastCriticalInteractionTime)/(float)PER_SEC<PAUSE_AFTER_CRITICAL_INTERACTION)
		{
			//lastCalcEndTime = 0;
			return RRScene::NOT_IMPROVED;
		}
	}

	// adjust userStep
	if(illuminationUse)
	{
		float lastUserStep = (calcBeginTime-lastCalcEndTime)/(float)PER_SEC;
		if(lastCalcEndTime && lastUserStep<1.0f)
		{
			if(!userStep)
				userStep = lastUserStep;
			else
				userStep = 0.6f*userStep + 0.4f*lastUserStep;
		}
	}

	// adjust improveStep
	if(!userStep || !calcStep || !improveStep || !illuminationUse)
	{
		improveStep = IMPROVE_STEP_NO_INTERACTION;
	}
	else
	{		
		// try to balance our (calculate) time and user time 1:1
		improveStep *= (calcStep>userStep)?0.8f:1.2f;
		// always spend at least 40% of our time in improve, don't spend too much by reading results etc
		improveStep = MAX(improveStep,MIN_PORTION_SPENT_IMPROVING*calcStep);
		// stick in interactive bounds
		improveStep = CLAMP(improveStep,IMPROVE_STEP_MIN,IMPROVE_STEP_MAX);
	}

	// calculate
	RRScene::Improvement result = calculateCore(improveStep);

	// adjust calcStep
	lastCalcEndTime = GETTIME;
	float lastCalcStep = (lastCalcEndTime-calcBeginTime)/(float)PER_SEC;
	if(lastCalcStep<1.0)
	{
		if(!calcStep)
			calcStep = lastCalcStep;
		else
			calcStep = 0.6f*calcStep + 0.4f*lastCalcStep;
	}

	return result;
}

} // namespace
