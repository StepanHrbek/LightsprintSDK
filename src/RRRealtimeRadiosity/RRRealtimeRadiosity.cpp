#define MULTIOBJECT // creates multiObject to accelerate calculation

#include <assert.h>
#include <float.h> // _finite
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include <time.h>
#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Timer.h"

namespace rr
{

// times in seconds
#define PAUSE_AFTER_CRITICAL_INTERACTION 0.2f // stops calculating after each interaction, improves responsiveness
#define IMPROVE_STEP_MIN 0.005f
#define IMPROVE_STEP_MAX 0.08f
#define IMPROVE_STEP_NO_INTERACTION 0.1f // length of one improve step when there are no interactions from user
#define IMPROVE_STEP_MIN_AFTER_BIG_RESET 0.035f // minimal length of first improve step after big reset (to avoid 1frame blackouts)
#define READING_RESULTS_PERIOD_MIN 0.1f // how often results are readen back when scene doesn't change
#define READING_RESULTS_PERIOD_MAX 1.5f //
#define READING_RESULTS_PERIOD_GROWTH 1.3f // reading results period is increased this times at each read since scene change
// portions in <0..1>
#define MIN_PORTION_SPENT_IMPROVING 0.4f // at least 40% of our time is spent in improving
// kdyz se jen renderuje a improvuje (rrbugs), az do 0.6 roste vytizeni cpu(dualcore) a nesnizi se fps
// kdyz se navic detekuje primary, kazde zvyseni snizi fps

#define REPORT(a)       //a
#define REPORT_BEGIN(a) REPORT( Timer timer; timer.Start(); reportAction(a ".."); )
#define REPORT_END      REPORT( {char buf[20]; sprintf(buf," %d ms.\n",(int)(timer.Watch()*1000));reportAction(buf);} )

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMP(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

RRRealtimeRadiosity::RRRealtimeRadiosity()
{
	//objects zeroed by constructor
	multiObject = NULL;
	scene = NULL;
	stitchDistance = -1;
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = BIG_CHANGE;
	dirtyResults = true;
	lastIlluminationUseTime = 0;
	lastCriticalInteractionTime = 0;
	lastCalcEndTime = 0;
	lastReadingResultsTime = 0;
	userStep = 0;
	calcStep = 0;
	improveStep = 0;
	readingResultsPeriod = 0;
	multiObjectBase = NULL;
	//preVertex2PostTriangleVertex zeroed by constructor
	resultChannelIndex = 0;
	rr::RRScene::setStateF(rr::RRScene::SUBDIVISION_SPEED,0);
	rr::RRScene::setState(rr::RRScene::GET_SOURCE,0);
	rr::RRScene::setStateF(rr::RRScene::MIN_FEATURE_SIZE,0.15f);
	timeBeginPeriod(1); // improves precision of demoengine's GETTIME
}

RRRealtimeRadiosity::~RRRealtimeRadiosity()
{
	delete scene->getScaler();
	delete scene;
	delete multiObject;
	delete multiObjectBase;
}

void RRRealtimeRadiosity::setObjects(Objects& aobjects, float astitchDistance)
{
	objects = aobjects;
	stitchDistance = astitchDistance;
	dirtyGeometry = true;
}

unsigned RRRealtimeRadiosity::getNumObjects() const
{
	return (unsigned)objects.size();
}

RRObject* RRRealtimeRadiosity::getObject(unsigned i)
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	if(i>=objects.size()) return NULL;
	return objects.at(i).first;
}

RRObjectAdditionalIllumination* RRRealtimeRadiosity::getMultiObject()
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	return multiObject;
}

const RRScene* RRRealtimeRadiosity::getScene()
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	return scene;
}

RRObjectIllumination* RRRealtimeRadiosity::getIllumination(unsigned i)
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	if(i>=objects.size()) return NULL;
	return objects.at(i).second;
}


void RRRealtimeRadiosity::adjustScene()
{
	delete scene->getScaler();
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

RRIlluminationVertexBuffer* RRRealtimeRadiosity::newVertexBuffer(unsigned numVertices)
{
	return RRIlluminationVertexBuffer::createInSystemMemory(numVertices);
}

void RRRealtimeRadiosity::readVertexResults()
{
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
					scene->getSubtriangleMeasure(0,postImportTriangle,RM_IRRADIANCE,renderSubtriangle,&rsc);
				}
#else
				scene->getSubtriangleMeasure(objectHandle,postImportTriangle,RM_IRRADIANCE,renderSubtriangle,&rsc)
#endif
			}
			pixelBuffer->renderEnd();
		}
	}
}

RRScene::Improvement RRRealtimeRadiosity::calculateCore(unsigned requests, float improveStep)
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
			delete multiObject;
			delete multiObjectBase;
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
		multiObjectBase = RRObject::createMultiObject(importers,(unsigned)objects.size(),RRCollider::IT_BSP_FASTEST,stitchDistance,stitchDistance>=0,NULL);
		//scene->setStateF(RRScene::IGNORE_SMALLER_ANGLE,-1);
		//scene->setStateF(RRScene::IGNORE_SMALLER_AREA,-1);
		//scene->setStateF(RRScene::MAX_SMOOTH_ANGLE,0.6f);
		//scene->setStateF(RRScene::MIN_FEATURE_SIZE,-0.1f);
		multiObject = multiObjectBase ? multiObjectBase->createAdditionalIllumination() : NULL;
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
		dirtyResults = true;
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
		if(dirtyEnergies==BIG_CHANGE)
		{
			// following improvement should be so big that single frames after big reset are not visibly darker
			// so...calculate at least 20ms?
			improveStep = MAX(improveStep,IMPROVE_STEP_MIN_AFTER_BIG_RESET);
		}
		dirtyEnergies = NO_CHANGE;
		dirtyResults = true;
	}

	REPORT_BEGIN("Calculating.");
	TIME now = GETTIME;
	TIME end = (TIME)(now+improveStep*PER_SEC);
	RRScene::Improvement improvement = scene->illuminationImprove(endByTime,(void*)&end);
	REPORT(printf(" (imp %d det+res+read %d game %d) ",(int)(1000*improveStep),(int)(1000*calcStep-improveStep),(int)(1000*userStep)));
	REPORT_END;
	switch(improvement)
	{
		case RRScene::IMPROVED:
			dirtyResults = true;
			break;
		case RRScene::NOT_IMPROVED:
			break;
		case RRScene::FINISHED:
		case RRScene::INTERNAL_ERROR:
			if(!dirtyResults) return improvement;
			break;
	}

	if((dirtyResults && now>=(TIME)(lastReadingResultsTime+readingResultsPeriod*PER_SEC))
		|| (requests&(UPDATE_VERTEX_BUFFERS|UPDATE_PIXEL_BUFFERS)) )
	{
		dirtyResults = false;
		REPORT_BEGIN("Reading results.");
		lastReadingResultsTime = now;
		if(readingResultsPeriod<READING_RESULTS_PERIOD_MAX) readingResultsPeriod *= READING_RESULTS_PERIOD_GROWTH;
		readVertexResults();
		readPixelResults();
		REPORT_END;
		return RRScene::IMPROVED;
	}
	return RRScene::NOT_IMPROVED;
}

RRScene::Improvement RRRealtimeRadiosity::calculate(unsigned requests)
{
	TIME calcBeginTime = GETTIME;
	bool illuminationUse = lastIlluminationUseTime && lastIlluminationUseTime>=lastCalcEndTime;
	//printf("%f %f %f\n",calcBeginTime*1.0f,lastIlluminationUseTime*1.0f,lastCalcEndTime*1.0f);

	// pause during critical interactions
	//  but avoid early return in case update was manually requested
	if(lastCriticalInteractionTime && !(requests&(UPDATE_VERTEX_BUFFERS|UPDATE_PIXEL_BUFFERS)))
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
		if(!lastUserStep) lastUserStep = 0.00001f; // fight with low timer precision, avoid 0, initial userStep=0 means 'unknown yet' which forces too long improve (IMPROVE_STEP_NO_INTERACTION)
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
	RRScene::Improvement result = calculateCore(requests,improveStep);

	// adjust calcStep
	lastCalcEndTime = GETTIME;
	float lastCalcStep = (lastCalcEndTime-calcBeginTime)/(float)PER_SEC;
	if(!lastCalcStep) lastCalcStep = 0.00001f; // fight low timer precision, avoid 0, initial calcStep=0 means 'unknown yet'
	if(lastCalcStep<1.0)
	{
		if(!calcStep)
			calcStep = lastCalcStep;
		else
			calcStep = 0.6f*calcStep + 0.4f*lastCalcStep;
	}

	return result;
}

/////////////////////////////////////////////////////////////////////////////
//
// Cube maps

// navaznost hran v cubemape:
// side0top   = side2right
// side0left  = side4right
// side0right = side5left
// side0bot   = side3right
// side1top   = side2left
// side1left  = side5right
// side1right = side4left
// side1bot   = side3left
// side2top   = side5top
// side2left  = side1top
// side2right = side0top
// side2bot   = side4top
// side3top   = side4bot
// side3left  = side1bot
// side3right = side0bot
// side3bot   = side5bot
// side4top   = side2bot
// side4left  = side1right
// side4right = side0left
// side4bot   = side3top
// side5top   = side2top
// side5left  = side0right
// side5right = side1left
// side5bot   = side3bot

enum Edge {TOP=0, LEFT, RIGHT, BOTTOM};

struct CubeSide
{
	signed char dir[3];
	struct Neighbour
	{
		char side;
		char edge;
	};
	Neighbour neighbour[4]; // indexed by Edge

	RRVec3 getDir() const
	{
		return RRVec3(dir[0],dir[1],dir[2]);
	}

	// returns direction from cube center to cube texel center.
	// texel coordinates: -1..1
	RRVec3 getTexelDir(RRVec2 uv) const;

	// returns direction from cube center to cube texel center.
	// texel coordinates: 0..size-1
	RRVec3 getTexelDir(unsigned size, unsigned x, unsigned y) const
	{
		return getTexelDir(RRVec2( RRReal(1+2*x-size)/size , RRReal(1+2*y-size)/size ));
	}
};

static const CubeSide cubeSide[6] =
{
	{{ 1, 0, 0}, {{2,RIGHT },{4,RIGHT },{5,LEFT  },{3,RIGHT }}},
	{{-1, 0, 0}, {{2,LEFT  },{5,RIGHT },{4,LEFT  },{3,LEFT  }}},
	{{ 0, 1, 0}, {{5,TOP   },{1,TOP   },{0,TOP   },{4,TOP   }}},
	{{ 0,-1, 0}, {{4,BOTTOM},{1,BOTTOM},{0,BOTTOM},{5,BOTTOM}}},
	{{ 0, 0, 1}, {{2,BOTTOM},{1,RIGHT },{0,LEFT  },{3,TOP   }}},
	{{ 0, 0,-1}, {{2,TOP   },{0,RIGHT },{1,LEFT  },{3,BOTTOM}}}
};

RRVec3 CubeSide::getTexelDir(RRVec2 uv) const
{
	return getDir()+cubeSide[neighbour[RIGHT].side].getDir()*uv[0]+cubeSide[neighbour[BOTTOM].side].getDir()*uv[1];
}

// inputs:
//  iSize - input cube size
//  iIrradiance - array with 6*iSize*iSize pixels of cube
// outputs:
//  if successful:
//   oSize - new filtered cube size
//   oIrradiance - new array with filtered cube, to be deleted by caller
//  else
//   not modified
static void cubeMapFilter(unsigned iSize, RRColorRGBA8* iIrradiance, unsigned& oSize, RRColorRGBA8*& oIrradiance)
{
	unsigned iPixels = iSize*iSize*6;
	if(iSize==1)
	{
		static const signed char filteringTable[] = {
			// side0
			1,0,1,0,1,0,
			1,0,1,0,0,1,
			1,0,0,1,1,0,
			1,0,0,1,0,1,
			// side1
			0,1,1,0,0,1,
			0,1,1,0,1,0,
			0,1,0,1,0,1,
			0,1,0,1,1,0,
			// side2
			0,1,1,0,0,1,
			1,0,1,0,0,1,
			0,1,1,0,1,0,
			1,0,1,0,1,0,
			// side3
			0,1,0,1,1,0,
			1,0,0,1,1,0,
			0,1,0,1,0,1,
			1,0,0,1,0,1,
			// side4
			0,1,1,0,1,0,
			1,0,1,0,1,0,
			0,1,0,1,1,0,
			1,0,0,1,1,0,
			// side5
			1,0,1,0,0,1,
			0,1,1,0,0,1,
			1,0,0,1,0,1,
			0,1,0,1,0,1,
		};
		oSize = 2;
		unsigned oPixels = 6*oSize*oSize;
		oIrradiance = new RRColorRGBA8[oPixels];
		for(unsigned i=0;i<oPixels;i++)
		{
			RRColorRGBF sum = RRColorRGBF(0);
			unsigned points = 0;
			for(unsigned j=0;j<iPixels;j++)
			{
				sum += iIrradiance[j].toRRColorRGBF() * filteringTable[iPixels*i+j];
				points += filteringTable[iPixels*i+j];
			}
			assert(points);
			oIrradiance[i] = sum / (points?points:1);
		}
	}
}

// thread safe: yes
void cubeMapGather(const RRScene* scene, const RRObject* object, RRVec3 center, unsigned size, RRColorRGBA8* irradiance)
{
/*
	irradiance[0].color = (rand()&1)?0xff0000:0x770000;
	irradiance[1].color = (rand()&1)?0x00ff00:0x007700;
	irradiance[2].color = 0x0000ff;
	irradiance[3].color = 0xffffff;
	irradiance[4].color = 0x00ffff;
	irradiance[5].color = 0xff00ff;
*/
	RRRay* ray = RRRay::create();
	for(unsigned side=0;side<6;side++)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
			{
				RRVec3 dir = cubeSide[side].getTexelDir(size,i,j);
				RRReal dirsize = dir.length();
				// find face
				ray->rayOrigin = center;
				ray->rayDirInv[0] = dirsize/dir[0];
				ray->rayDirInv[1] = dirsize/dir[1];
				ray->rayDirInv[2] = dirsize/dir[2];
				ray->rayLengthMin = 0;
				ray->rayLengthMax = 10000; //!!! hard limit
				ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::TEST_SINGLESIDED;
				unsigned face = object->getCollider()->intersect(ray) ? ray->hitTriangle : UINT_MAX;
				// read irradiance on sky
				RRVec3 irrad;
				if(face==UINT_MAX)
				{
					irrad = RRVec3(0); //!!! add sky
				}
				else
				// read irradiance on face
				{
					scene->getTriangleMeasure(0,face,3,RM_IRRADIANCE,irrad);
				}
				// write result
				*irradiance++ = irrad;
			}
	}
	delete ray;
}

void RRRealtimeRadiosity::updateEnvironmentMap(RRIlluminationEnvironmentMap* environmentMap, RRVec3 objectCenterWorld)
{
	if(!environmentMap) return;

	// gather irradiances
	const unsigned iSize = 1;
	RRColorRGBA8 iIrradiance[6*iSize*iSize];
	cubeMapGather(scene,multiObject,objectCenterWorld,iSize,iIrradiance);

	// filter cubemap
	unsigned oSize = 0;
	RRColorRGBA8* oIrradiance = NULL;
	cubeMapFilter(1,iIrradiance,oSize,oIrradiance);

	// pass cubemap to client
	environmentMap->setValues(oSize?oSize:iSize,oIrradiance?oIrradiance:iIrradiance);

	// cleanup
	delete oIrradiance;
}

} // namespace
