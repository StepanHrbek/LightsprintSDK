#define MULTIOBJECT // creates multiObject to accelerate calculation

#include <assert.h>
#include <float.h> // _finite
#include <time.h>
#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Timer.h"


namespace rr
{

// times in seconds
#define PAUSE_AFTER_CRITICAL_INTERACTION 0.2f // stops calculating after each interaction, improves responsiveness
#define IMPROVE_STEP_NO_INTERACTION 0.1f // length of one improve step when there are no interactions from user
#define IMPROVE_STEP_MIN 0.005f
#define IMPROVE_STEP_MAX 0.08f
#define READING_RESULTS_PERIOD_MIN 0.1f // how often results are readen back when scene doesn't change
#define READING_RESULTS_PERIOD_MAX 1.5f //
#define READING_RESULTS_PERIOD_GROWTH 1.3f // reading results period is increased this times at each read since scene change
// portions in <0..1>
#define MIN_PORTION_SPENT_IMPROVING 0.4f // at least 40% of our time is spent in improving

#define REPORT(a)       //a
#define REPORT_BEGIN(a) REPORT( Timer timer; timer.Start(); reportAction(a ".."); )
#define REPORT_END      REPORT( {char buf[20]; sprintf(buf," %d ms.\n",(int)(timer.Watch()*1000));reportAction(buf);} )

// odsunout do RRIlluminationPixelBuffer.cpp
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMP(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

RRRealtimeRadiosity::RRRealtimeRadiosity()
{
	multiObject = NULL;
	scene = NULL;
	//objects zeroed by constructor
	stitchDistance = -1;
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = BIG_CHANGE;
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
	rr::RRScene::setStateF(rr::RRScene::SUBDIVISION_SPEED,0);
	rr::RRScene::setState(rr::RRScene::GET_SOURCE,0);
	rr::RRScene::setStateF(rr::RRScene::MIN_FEATURE_SIZE,0.15f);
}

RRRealtimeRadiosity::~RRRealtimeRadiosity()
{
	delete scene;
}

void RRRealtimeRadiosity::setObjects(Objects& aobjects, float astitchDistance)
{
	objects = aobjects;
	stitchDistance = astitchDistance;
	dirtyGeometry = true;
}

unsigned RRRealtimeRadiosity::getNumObjects()
{
	return (unsigned)objects.size();
}

RRObject* RRRealtimeRadiosity::getObject(unsigned i)
{
	if(i>=objects.size()) return NULL;
	return objects.at(i).first;
}

const RRObjectAdditionalIllumination* RRRealtimeRadiosity::getMultiObject()
{
	return multiObject;
}

const RRScene* RRRealtimeRadiosity::getScene()
{
	return scene;
}

RRObjectIllumination* RRRealtimeRadiosity::getIllumination(unsigned i)
{
	if(i>=objects.size()) return NULL;
	return objects.at(i).second;
}


void RRRealtimeRadiosity::adjustScene()
{
	scene->setScaler(RRScaler::createRgbScaler(0.4f));
}

void RRRealtimeRadiosity::reportAction(const char* action) const
{
}

void RRRealtimeRadiosity::reportMaterialChange()
{
	REPORT(reportAction("<MaterialChange>"));
	dirtyMaterials = true;
}

void RRRealtimeRadiosity::reportGeometryChange()
{
	REPORT(reportAction("<GeometryChange>"));
	dirtyGeometry = true;
}

void RRRealtimeRadiosity::reportLightChange(bool strong)
{
	REPORT(reportAction(strong?"<LightChangeStrong>":"<LightChange>"));
	dirtyLights = strong?BIG_CHANGE:SMALL_CHANGE;
}

void RRRealtimeRadiosity::reportIlluminationUse()
{
	REPORT(reportAction("<IlluminationUse>"));
	lastIlluminationUseTime = GETTIME;
}

void RRRealtimeRadiosity::reportCriticalInteraction()
{
	REPORT(reportAction("<CriticalInteraction>"));
	lastCriticalInteractionTime = GETTIME;
}

void RRRealtimeRadiosity::reportEndOfInteractions()
{
	REPORT(reportAction("<EndOfInteractions>"));
	lastCriticalInteractionTime = 0;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

void RRRealtimeRadiosity::updateVertexLookupTable()
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

void RRRealtimeRadiosity::readVertexResults()
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


RRScene::Improvement RRRealtimeRadiosity::calculateCore(float improveStep)
{
	bool dirtyFactors = false;
	ChangeStrength dirtyEnergies = NO_CHANGE;
	if(dirtyMaterials)
	{
		dirtyMaterials = false;
		dirtyFactors = true;
		REPORT_BEGIN("Detecting material properties.");
		detectMaterials();
		REPORT_END;
	}
	if(dirtyGeometry)
	{
		dirtyGeometry = false;
		dirtyLights = BIG_CHANGE;
		dirtyFactors = true;
		if(scene)
		{
			REPORT_BEGIN("Closing old radiosity solver.");
			delete scene;
			REPORT_END;
		}
		REPORT_BEGIN("Opening new radiosity solver.");
		scene = new RRScene;
#ifdef MULTIOBJECT
		RRObject** importers = new RRObject*[objects.size()];
		for(unsigned i=0;i<(unsigned)objects.size();i++)
		{
			importers[i] = objects.at(i).first;
		}
		RRObject* object = RRObject::createMultiObject(importers,(unsigned)objects.size(),RRCollider::IT_BSP_FASTEST,stitchDistance,stitchDistance>=0,NULL);
		//scene->setStateF(RRScene::IGNORE_SMALLER_ANGLE,-1);
		//scene->setStateF(RRScene::IGNORE_SMALLER_AREA,-1);
		//scene->setStateF(RRScene::MAX_SMOOTH_ANGLE,0.6f);
		//scene->setStateF(RRScene::MIN_FEATURE_SIZE,-0.1f);
		multiObject = object ? object->createAdditionalIllumination() : NULL;
		delete[] importers;
		scene->objectCreate(multiObject);
#else
		for(Objects::iterator i=objects.begin();i!=objects.end();i++)
			scene->objectCreate((*i).first);
#endif
		adjustScene();
		updateVertexLookupTable();
		REPORT_END;
	}
	if(dirtyLights!=NO_CHANGE)
	{
		dirtyEnergies = dirtyLights;
		dirtyLights = NO_CHANGE;
		readingResultsPeriod = READING_RESULTS_PERIOD_MIN;
		REPORT_BEGIN("Detecting direct illumination.");
		if(!detectDirectIllumination())
		{
			// detection has failed, ensure these points:
			// 1) detection will be detected again next time
			dirtyLights = BIG_CHANGE;
			// 2) eventual dirtyFactors = true; won't be forgotten
			// let normal dirtyFactors handler work now, exit later
			// 3) no calculations on current obsoleted primaries will be wasted
			// exit before resetting energies
			// exit before factor calculation and energy propagation
		}
		REPORT_END;
	}
	if(dirtyFactors)
	{
		dirtyFactors = false;
		dirtyEnergies = NO_CHANGE;
		REPORT_BEGIN("Resetting solver energies and factors.");
		scene->illuminationReset(true,true);
		REPORT_END;
	}
	if(dirtyLights!=NO_CHANGE)
	{
		// exit in response to unsuccessful detectDirectIllumination
		return RRScene::NOT_IMPROVED;
	}
	if(dirtyEnergies!=NO_CHANGE)
	{
		REPORT_BEGIN("Updating solver energies.");
		scene->illuminationReset(false,dirtyEnergies==BIG_CHANGE);
		REPORT_END;
		dirtyEnergies = NO_CHANGE;
	}

	REPORT_BEGIN("Calculating.");
	TIME now = GETTIME;
	TIME end = (TIME)(now+improveStep*PER_SEC);
	RRScene::Improvement improvement = scene->illuminationImprove(endByTime,(void*)&end);
	if(improvement==RRScene::FINISHED || improvement==RRScene::INTERNAL_ERROR)
		return improvement;
	REPORT(printf(" (imp %d det+res+read %d game %d) ",(int)(1000*improveStep),(int)(1000*calcStep-improveStep),(int)(1000*userStep)));
	REPORT_END;

	if(now>=(TIME)(lastReadingResultsTime+readingResultsPeriod*PER_SEC))
	{
		REPORT_BEGIN("Reading results.");
		lastReadingResultsTime = now;
		if(readingResultsPeriod<READING_RESULTS_PERIOD_MAX) readingResultsPeriod *= READING_RESULTS_PERIOD_GROWTH;
		readVertexResults();
		REPORT_END;
		//readPixelResults();//!!!
		return RRScene::IMPROVED;
	}
	return RRScene::NOT_IMPROVED;
}

RRScene::Improvement RRRealtimeRadiosity::calculate()
{
	TIME calcBeginTime = GETTIME;
	bool illuminationUse = lastIlluminationUseTime && lastIlluminationUseTime>=lastCalcEndTime;
	//printf("%f %f %f\n",calcBeginTime*1.0f,lastIlluminationUseTime*1.0f,lastCalcEndTime*1.0f);

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
		REPORT(printf("User %d ms.\n",(int)(1000*lastUserStep)));
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
