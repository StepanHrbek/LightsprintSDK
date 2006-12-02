#define MULTIOBJECT // creates multiObject to accelerate calculation

#include <assert.h>
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
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = BIG_CHANGE;
	dirtyResults = true;
	lastInteractionTime = 0;
	lastCalcEndTime = 0;
	lastReadingResultsTime = 0;
	userStep = 0;
	calcStep = 0;
	improveStep = 0;
	readingResultsPeriod = 0;
	multiObjectBase = NULL;
	//preVertex2PostTriangleVertex zeroed by constructor
	resultChannelIndex = 0;
	timeBeginPeriod(1); // improves precision of demoengine's GETTIME
}

RRRealtimeRadiosity::~RRRealtimeRadiosity()
{
	if(scene)
	{
		delete scene->getScaler();
		delete scene;
	}
	delete multiObject;
	delete multiObjectBase;
}

void RRRealtimeRadiosity::setObjects(Objects& aobjects, const RRScene::SmoothingParameters* asmoothing)
{
	objects = aobjects;
	smoothing = asmoothing ? *asmoothing : RRScene::SmoothingParameters();
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


void RRRealtimeRadiosity::onSceneInit()
{
}

void RRRealtimeRadiosity::reportMaterialChange()
{
	REPORT(reportAction("<MaterialChange>"));
	dirtyMaterials = true;
}

void RRRealtimeRadiosity::reportLightChange(bool strong)
{
	REPORT(reportAction(strong?"<LightChangeStrong>":"<LightChange>"));
	dirtyLights = strong?BIG_CHANGE:SMALL_CHANGE;
}

void RRRealtimeRadiosity::reportInteraction()
{
	REPORT(reportAction("<Interaction>"));
	lastInteractionTime = GETTIME;
}


static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

// calculates radiosity in existing times (improveStep = time to spend in improving),
//  does no timing adjustments
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
		scene = new RRScene();
#ifdef MULTIOBJECT
		RRObject** importers = new RRObject*[objects.size()];
		for(unsigned i=0;i<(unsigned)objects.size();i++)
		{
			importers[i] = objects.at(i).first;
		}
		multiObjectBase = RRObject::createMultiObject(importers,(unsigned)objects.size(),RRCollider::IT_BSP_FASTEST,smoothing.stitchDistance,smoothing.stitchDistance>=0,NULL);
		multiObject = multiObjectBase ? multiObjectBase->createAdditionalIllumination() : NULL;
		delete[] importers;
		scene->objectCreate(multiObject,&smoothing);
#else
		for(Objects::iterator i=objects.begin();i!=objects.end();i++)
			scene->objectCreate((*i).first,smoothing);
#endif
		onSceneInit();
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
		if(scene) scene->illuminationReset(true,true);
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
		if(scene) scene->illuminationReset(false,dirtyEnergies==BIG_CHANGE);
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
	RRScene::Improvement improvement = scene ? scene->illuminationImprove(endByTime,(void*)&end) : RRScene::FINISHED;
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

// adjusts timing, does no radiosity calculation (but calls calculateCore that does)
RRScene::Improvement RRRealtimeRadiosity::calculate(unsigned requests)
{
	TIME calcBeginTime = GETTIME;
	//printf("%f %f %f\n",calcBeginTime*1.0f,lastInteractionTime*1.0f,lastCalcEndTime*1.0f);


	// adjust userStep
	float lastUserStep = (calcBeginTime-lastCalcEndTime)/(float)PER_SEC;
	if(!lastUserStep) lastUserStep = 0.00001f; // fight with low timer precision, avoid 0, initial userStep=0 means 'unknown yet' which forces too long improve (IMPROVE_STEP_NO_INTERACTION)
	if(lastInteractionTime && lastInteractionTime>=lastCalcEndTime)
	{
		// reportInteraction was called between this and previous calculate
		if(lastCalcEndTime && lastUserStep<1.0f)
		{
			userStep = lastUserStep;
		}
		REPORT(printf("User %d ms.\n",(int)(1000*lastUserStep)));
	} else {
		// no reportInteraction was called between this and previous calculate
		// -> increase userStep
		//    (this slowly increases calculation time)
		userStep = lastInteractionTime ?
			(lastCalcEndTime-lastInteractionTime)/(float)PER_SEC // time from last interaction (if there was at least one)
			: IMPROVE_STEP_NO_INTERACTION; // safety time for situations there was no interaction yet
		REPORT(printf("User %d ms (accumulated to %d).\n",(int)(1000*lastUserStep),(int)(1000*userStep)));
	}

	// adjust improveStep
	if(!userStep || !calcStep || !improveStep)
	{
		REPORT(printf("Reset to NO_INTERACT(%f,%f,%f).",userStep,calcStep,improveStep));
		improveStep = IMPROVE_STEP_NO_INTERACTION;
	}
	else
	{		
		// try to balance our (calculate) time and user time 1:1
		improveStep *= 
			// kdyz byl userTime 1000 a nahle spadne na 1 (protoze user hmatnul na klavesy),
			// toto zajisti rychly pad improveStep z 80 na male hodnoty zajistujici plynuly render,
			// takze napr. rozjezd kamery nezacne strasnym cukanim
			(calcStep>6*userStep)?0.4f: 
			// standardni vyvazovani s tlumenim prudkych vykyvu
			( (calcStep>userStep)?0.8f:1.2f );
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

unsigned RR_INTERFACE_ID_LIB()
{
	return RR_INTERFACE_ID_APP();
}

char* RR_INTERFACE_DESC_LIB()
{
	return RR_INTERFACE_DESC_APP();
}

} // namespace
