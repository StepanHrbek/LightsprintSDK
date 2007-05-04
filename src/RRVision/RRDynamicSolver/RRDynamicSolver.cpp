#include <assert.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "report.h"

namespace rr
{

// times in seconds
#define PAUSE_AFTER_CRITICAL_INTERACTION 0.2f // stops calculating after each interaction, improves responsiveness
#define IMPROVE_STEP_MIN 0.005f
#define IMPROVE_STEP_MAX 0.08f
#define IMPROVE_STEP_NO_INTERACTION 0.1f // length of one improve step when there are no interactions from user
#define IMPROVE_STEP_MIN_AFTER_BIG_RESET 0.035f // minimal length of first improve step after big reset (to avoid 1frame blackouts)
#define READING_RESULTS_PERIOD_MIN 0.1f // how often results are read back when scene doesn't change
#define READING_RESULTS_PERIOD_MAX 1.5f //
#define READING_RESULTS_PERIOD_GROWTH 1.3f // reading results period is increased this times at each read since scene change
// portions in <0..1>
#define MIN_PORTION_SPENT_IMPROVING 0.4f // at least 40% of our time is spent in improving
// kdyz se jen renderuje a improvuje (rrbugs), az do 0.6 roste vytizeni cpu(dualcore) a nesnizi se fps
// kdyz se navic detekuje primary, kazde zvyseni snizi fps


#define MAX(a,b)         (((a)>(b))?(a):(b))
#define MIN(a,b)         (((a)<(b))?(a):(b))
#define CLAMP(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))
#define SAFE_DELETE(a)   {delete a;a=NULL;}

RRDynamicSolver::RRDynamicSolver()
{
	//objects zeroed by constructor
	scaler = NULL;
	environment = NULL;
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
	multiObjectCustom = NULL;
	multiObjectPhysical = NULL;
	multiObjectPhysicalWithIllumination = NULL;
	solutionVersion = 1;
	//preVertex2PostTriangleVertex zeroed by constructor
	timeBeginPeriod(1); // improves precision of demoengine's GETTIME
}

RRDynamicSolver::~RRDynamicSolver()
{
	delete scene;
	delete multiObjectPhysicalWithIllumination;
	if(multiObjectPhysical!=multiObjectCustom) delete multiObjectPhysical; // no scaler -> physical == custom
	delete multiObjectCustom;
}

void RRDynamicSolver::setScaler(RRScaler* ascaler)
{
	scaler = ascaler;
}

const RRScaler* RRDynamicSolver::getScaler() const
{
	return scaler;
}

void RRDynamicSolver::setEnvironment(RRIlluminationEnvironmentMap* aenvironment)
{
	environment = aenvironment;
}

const RRIlluminationEnvironmentMap* RRDynamicSolver::getEnvironment() const
{
	return environment;
}

void RRDynamicSolver::setLights(const RRLights& alights)
{
	lights = alights;
}

const RRLights& RRDynamicSolver::getLights() const
{
	return lights;
}

void RRDynamicSolver::setObjects(RRObjects& aobjects, const RRStaticSolver::SmoothingParameters* asmoothing)
{
	objects = aobjects;
	smoothing = asmoothing ? *asmoothing : RRStaticSolver::SmoothingParameters();
	dirtyGeometry = true;
}

unsigned RRDynamicSolver::getNumObjects() const
{
	return (unsigned)objects.size();
}

RRObject* RRDynamicSolver::getObject(unsigned i)
{
	// this is commented out, getObject() is allowed even immediately after setObjects()
	//if(dirtyGeometry) return NULL;

	if(i>=objects.size()) return NULL;
	return objects.at(i).object;
}

const RRObject* RRDynamicSolver::getMultiObjectCustom() const
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	return multiObjectCustom;
}

const RRObjectWithPhysicalMaterials* RRDynamicSolver::getMultiObjectPhysical() const
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	return multiObjectPhysical;
}

RRObjectWithIllumination* RRDynamicSolver::getMultiObjectPhysicalWithIllumination()
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	return multiObjectPhysicalWithIllumination;
}

const RRStaticSolver* RRDynamicSolver::getStaticSolver() const
{
	if(dirtyGeometry) return NULL; // setObjects() must be followed by calculate(), otherwise we are inconsistent
	return scene;
}

RRObjectIllumination* RRDynamicSolver::getIllumination(unsigned i)
{
	// this is commented out, getIllumination() is allowed even immediately after setObjects()
	//if(dirtyGeometry) return NULL;

	if(i>=objects.size()) return NULL;
	return objects.at(i).illumination;
}

const RRObjectIllumination* RRDynamicSolver::getIllumination(unsigned i) const
{
	// this is commented out, getIllumination() is allowed even immediately after setObjects()
	//if(dirtyGeometry) return NULL;

	if(i>=objects.size()) return NULL;
	return objects.at(i).illumination;
}

void RRDynamicSolver::reportMaterialChange()
{
	REPORT(RRReporter::report(RRReporter::INFO,"<MaterialChange>\n"));
	dirtyMaterials = true;
}

void RRDynamicSolver::reportDirectIlluminationChange(bool strong)
{
	REPORT(RRReporter::report(RRReporter::INFO,strong?"<IlluminationChangeStrong>\n":"<IlluminationChange>\n"));
	dirtyLights = strong?BIG_CHANGE:SMALL_CHANGE;
}

void RRDynamicSolver::reportInteraction()
{
	REPORT(RRReporter::report(RRReporter::INFO,"<Interaction>\n"));
	lastInteractionTime = GETTIME;
}


static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

template <class C, int I, int InitialNoSmooth>
class Smoother
{
public:
	Smoother()
	{
		loaded = 0;
		i = 0;
		toSkip = InitialNoSmooth;
	}
	C smooth(C c)
	{
		if(toSkip)
		{
			toSkip--;
			return c;
		}
		if(loaded<I) loaded++;
		history[i] = c;
		i++; i%=I;
		C avg = 0;
		for(unsigned i=0;i<loaded;i++) avg += history[i];
		return avg/loaded;
	}
private:
	C history[I];
	unsigned loaded;
	unsigned i;
	unsigned toSkip;
};

// calculates radiosity in existing times (improveStep = seconds to spend in improving),
//  does no timing adjustments
RRStaticSolver::Improvement RRDynamicSolver::calculateCore(float improveStep)
{
	REPORT_INIT;
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
			SAFE_DELETE(scene);
			if(multiObjectPhysical!=multiObjectCustom) // no scaler -> physical == custom
				SAFE_DELETE(multiObjectCustom)
			else
				multiObjectCustom = NULL;
			SAFE_DELETE(multiObjectPhysical);
			SAFE_DELETE(multiObjectPhysicalWithIllumination);
			REPORT_END;
		}
		REPORT_BEGIN("Opening new radiosity solver.");
		RRObject** importers = new RRObject*[objects.size()];
		for(unsigned i=0;i<(unsigned)objects.size();i++)
		{
			importers[i] = objects.at(i).object;
		}
		// create multi in custom scale
		multiObjectCustom = RRObject::createMultiObject(importers,(unsigned)objects.size(),smoothing.intersectTechnique,smoothing.stitchDistance,smoothing.stitchDistance>=0,NULL);
		// convert it to physical scale
		multiObjectPhysical = (multiObjectCustom) ? multiObjectCustom->createObjectWithPhysicalMaterials(getScaler()) : NULL; // no scaler -> physical == custom
		//multiObjectPhysical = (multiObjectCustom&&getScaler()) ? multiObjectCustom->createObjectWithPhysicalMaterials(getScaler()) : NULL; // no scaler -> custom=1,physical=0
		// add direct illumination
		multiObjectPhysicalWithIllumination = multiObjectPhysical ? multiObjectPhysical->createObjectWithIllumination(getScaler()) : 
			(multiObjectCustom ? multiObjectCustom->createObjectWithIllumination(getScaler()) : NULL);
		delete[] importers;
		REPORT(if(multiObjectPhysicalWithIllumination)
			RRReporter::report(RRReporter::CONT,"(%d objects, optimized to %d faces, %d vertices) ",objects.size(),multiObjectPhysicalWithIllumination->getCollider()->getMesh()->getNumTriangles(),multiObjectPhysicalWithIllumination->getCollider()->getMesh()->getNumVertices()));
		scene = multiObjectPhysicalWithIllumination ? new RRStaticSolver(multiObjectPhysicalWithIllumination,&smoothing) : NULL;
		if(scene) updateVertexLookupTable();
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
		solutionVersion++;
		REPORT_END;
	}
	if(dirtyLights!=NO_CHANGE)
	{
		// exit in response to unsuccessful detectDirectIllumination
		return RRStaticSolver::NOT_IMPROVED;
	}
	if(dirtyEnergies!=NO_CHANGE)
	{
		REPORT_BEGIN("Updating solver energies.");
		if(scene) scene->illuminationReset(false,dirtyEnergies==BIG_CHANGE);
		solutionVersion++;
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
	RRStaticSolver::Improvement improvement = scene ? scene->illuminationImprove(endByTime,(void*)&end) : RRStaticSolver::FINISHED;
	//REPORT(RRReporter::report(RRReporter::CONT," (imp %d det+res+read %d game %d) ",(int)(1000*improveStep),(int)(1000*calcStep-improveStep),(int)(1000*userStep)));
	REPORT_END;
	switch(improvement)
	{
		case RRStaticSolver::IMPROVED:
			dirtyResults = true;
			improvement = RRStaticSolver::NOT_IMPROVED; // hide improvement until reading results
			break;
		case RRStaticSolver::NOT_IMPROVED:
			break;
		case RRStaticSolver::FINISHED:
		case RRStaticSolver::INTERNAL_ERROR:
			break;
	}

#if 0
	// show speed
	static Smoother<unsigned,20,3> s1,s2,s3;
	TIME thisFrameEnd = GETTIME;
	static TIME prevFrameEnd = 0;
	if(prevFrameEnd)
		printf("##### frame time %d+%d=%dms #####\n",s1.smooth((now - prevFrameEnd)*1000/PER_SEC),s2.smooth((thisFrameEnd - now)*1000/PER_SEC),s3.smooth((thisFrameEnd - prevFrameEnd)*1000/PER_SEC));
	prevFrameEnd = thisFrameEnd;
	reportDirectIlluminationChange(true);
#endif

	if(dirtyResults && now>=(TIME)(lastReadingResultsTime+readingResultsPeriod*PER_SEC))
	{
		lastReadingResultsTime = now;
		if(readingResultsPeriod<READING_RESULTS_PERIOD_MAX) readingResultsPeriod *= READING_RESULTS_PERIOD_GROWTH;
		dirtyResults = false;
		solutionVersion++;
		improvement = RRStaticSolver::IMPROVED;
	}
	return improvement;
}

// adjusts timing, does no radiosity calculation (but calls calculateCore that does)
RRStaticSolver::Improvement RRDynamicSolver::calculate()
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
		REPORT(RRReporter::report(RRReporter::INFO,"User %d ms.\n",(int)(1000*lastUserStep)));
	} else {
		// no reportInteraction was called between this and previous calculate
		// -> increase userStep
		//    (this slowly increases calculation time)
		userStep = lastInteractionTime ?
			(lastCalcEndTime-lastInteractionTime)/(float)PER_SEC // time from last interaction (if there was at least one)
			: IMPROVE_STEP_NO_INTERACTION; // safety time for situations there was no interaction yet
		REPORT(RRReporter::report(RRReporter::INFO,"User %d ms (accumulated to %d).\n",(int)(1000*lastUserStep),(int)(1000*userStep)));
	}

	// adjust improveStep
	if(!userStep || !calcStep || !improveStep)
	{
		REPORT(RRReporter::report(RRReporter::INFO,"Reset to NO_INTERACT(%f,%f,%f).\n",userStep,calcStep,improveStep));
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
	RRStaticSolver::Improvement result = calculateCore(improveStep);

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

unsigned RRDynamicSolver::getSolutionVersion() const
{
	return solutionVersion;
}

unsigned RR_INTERFACE_ID_LIB()
{
	return RR_INTERFACE_ID_APP();
}

const char* RR_INTERFACE_DESC_LIB()
{
	return RR_INTERFACE_DESC_APP();
}

} // namespace
