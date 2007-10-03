#include <assert.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "report.h"
#include "private.h"
#include "../RRStaticSolver/rrcore.h" // buildovani packed faktoru

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
	priv = new Private;
}

RRDynamicSolver::~RRDynamicSolver()
{
	delete priv;
}

void RRDynamicSolver::setScaler(RRScaler* ascaler)
{
	priv->scaler = ascaler;
	// update fast conversion table for our detectDirectIllumination
	for(unsigned i=0;i<256;i++)
	{
		rr::RRColor c(i*priv->boostDetectedDirectIllumination/255);
		if(ascaler) ascaler->getPhysicalScale(c);
		priv->customToPhysical[i] = c[0];
	}
}

const RRScaler* RRDynamicSolver::getScaler() const
{
	return priv->scaler;
}

void RRDynamicSolver::setEnvironment(RRIlluminationEnvironmentMap* aenvironment)
{
	priv->environment = aenvironment;
}

const RRIlluminationEnvironmentMap* RRDynamicSolver::getEnvironment() const
{
	return priv->environment;
}

void RRDynamicSolver::setLights(const RRLights& alights)
{
	priv->lights = alights;
}

const RRLights& RRDynamicSolver::getLights() const
{
	return priv->lights;
}

void RRDynamicSolver::setStaticObjects(RRObjects& _objects, const RRStaticSolver::SmoothingParameters* _smoothing)
{
	priv->objects = _objects;
	priv->smoothing = _smoothing ? *_smoothing : RRStaticSolver::SmoothingParameters();
	priv->dirtyStaticSolver = true;
	priv->dirtyLights = Private::BIG_CHANGE;

	// delete old

	SAFE_DELETE(priv->packedSolver);
	SAFE_DELETE(priv->scene);
	if(priv->multiObjectPhysical==priv->multiObjectCustom) priv->multiObjectCustom = NULL; // no scaler -> physical == custom
	SAFE_DELETE(priv->multiObjectCustom);
	SAFE_DELETE(priv->multiObjectPhysical);
	SAFE_DELETE(priv->multiObjectPhysicalWithIllumination);

	// create new

	// create multi in custom scale
	RRObject** importers = new RRObject*[priv->objects.size()];
	unsigned origNumVertices = 0;
	unsigned origNumTriangles = 0;
	for(unsigned i=0;i<(unsigned)priv->objects.size();i++)
	{
		importers[i] = priv->objects[i].object;
		origNumVertices += importers[i]->getCollider()->getMesh()->getNumVertices();
		origNumTriangles += importers[i]->getCollider()->getMesh()->getNumTriangles();
	}
	priv->multiObjectCustom = RRObject::createMultiObject(importers,(unsigned)priv->objects.size(),priv->smoothing.intersectTechnique,priv->smoothing.vertexWeldDistance,priv->smoothing.vertexWeldDistance>=0,NULL);
	delete[] importers;

	// convert it to physical scale
	priv->multiObjectPhysical = (priv->multiObjectCustom) ? priv->multiObjectCustom->createObjectWithPhysicalMaterials(getScaler()) : NULL; // no scaler -> physical == custom
	//multiObjectPhysical = (multiObjectCustom&&getScaler()) ? multiObjectCustom->createObjectWithPhysicalMaterials(getScaler()) : NULL; // no scaler -> custom=1,physical=0

	// add direct illumination
	priv->multiObjectPhysicalWithIllumination = priv->multiObjectPhysical ? priv->multiObjectPhysical->createObjectWithIllumination(getScaler()) : 
		(priv->multiObjectCustom ? priv->multiObjectCustom->createObjectWithIllumination(getScaler()) : NULL);
	REPORT(if(priv->multiObjectCustom)
		RRReporter::report(INF3,"Static scene set: %d objects, optimized %d->%d tris, %d->%d verts\n",priv->objects.size(),origNumTriangles,priv->multiObjectCustom->getCollider()->getMesh()->getNumTriangles(),origNumVertices,priv->multiObjectCustom->getCollider()->getMesh()->getNumVertices()));

	// update minimalSafeDistance
	if(priv->multiObjectCustom)
	{
		RRVec3 mini,maxi,center;
		priv->multiObjectCustom->getCollider()->getMesh()->getAABB(&mini,&maxi,&center);
		priv->minimalSafeDistance = (maxi-mini).avg()*1e-6f;
	}
}

void RRDynamicSolver::setDynamicObjects(RRObjects& _objects)
{
	priv->dobjects = _objects;
}

unsigned RRDynamicSolver::getNumObjects() const
{
	return (unsigned)priv->objects.size();
}

RRObject* RRDynamicSolver::getObject(unsigned i)
{
	if(i>=priv->objects.size()) return NULL;
	return priv->objects[i].object;
}

const RRObject* RRDynamicSolver::getMultiObjectCustom() const
{
	return priv->multiObjectCustom;
}

const RRObjectWithPhysicalMaterials* RRDynamicSolver::getMultiObjectPhysical() const
{
	return priv->multiObjectPhysical;
}

RRObjectWithIllumination* RRDynamicSolver::getMultiObjectPhysicalWithIllumination()
{
	return priv->multiObjectPhysicalWithIllumination;
}

const RRStaticSolver* RRDynamicSolver::getStaticSolver() const
{
	if(priv->dirtyStaticSolver) return NULL; // setStaticObjects() must be followed by calculate(), otherwise we are inconsistent
	return priv->scene;
}

RRObjectIllumination* RRDynamicSolver::getIllumination(unsigned i)
{
	if(i>=priv->objects.size()) return NULL;
	return priv->objects[i].illumination;
}

const RRObjectIllumination* RRDynamicSolver::getIllumination(unsigned i) const
{
	if(i>=priv->objects.size()) return NULL;
	return priv->objects[i].illumination;
}

void RRDynamicSolver::reportMaterialChange()
{
	REPORT(RRReporter::report(INF1,"<MaterialChange>\n"));
	priv->dirtyMaterials = true;
}

void RRDynamicSolver::reportDirectIlluminationChange(bool strong)
{
	REPORT(RRReporter::report(INF1,strong?"<IlluminationChangeStrong>\n":"<IlluminationChange>\n"));
	priv->dirtyLights = strong?Private::BIG_CHANGE:Private::SMALL_CHANGE;
}

void RRDynamicSolver::reportInteraction()
{
	REPORT(RRReporter::report(INF1,"<Interaction>\n"));
	priv->lastInteractionTime = GETTIME;
}

void RRDynamicSolver::setDirectIlluminationBoost(RRReal boost)
{
	priv->boostDetectedDirectIllumination = boost;
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
RRStaticSolver::Improvement RRDynamicSolver::calculateCore(float improveStep,CalculateParams* _params)
{
	// replace NULL by default parameters
	static CalculateParams s_params;
	if(!_params) _params = &s_params;

	bool dirtyFactors = false;
	Private::ChangeStrength dirtyEnergies = Private::NO_CHANGE;
	if(priv->dirtyMaterials)
	{
		priv->dirtyMaterials = false;
		dirtyFactors = true;
		//SAFE_DELETE(priv->packedSolver); intentionally not deleted, material change is not expected to unload packed solver (even though it becomes incorrect)
		REPORT(RRReportInterval report(INF3,"Detecting material properties...\n"));
		detectMaterials();
	}
	if(priv->dirtyStaticSolver && !priv->packedSolver)
	{
		REPORT(RRReportInterval report(INF3,"Opening new radiosity solver...\n"));
		priv->dirtyStaticSolver = false;
		priv->dirtyLights = Private::BIG_CHANGE;
		dirtyFactors = true;
		// create new
		priv->scene = priv->multiObjectPhysicalWithIllumination ? new RRStaticSolver(priv->multiObjectPhysicalWithIllumination,&priv->smoothing) : NULL;
		if(priv->scene) updateVertexLookupTableDynamicSolver();
	}
	if(priv->dirtyLights!=Private::NO_CHANGE)
	{
		dirtyEnergies = priv->dirtyLights;
		priv->dirtyLights = Private::NO_CHANGE;
		priv->readingResultsPeriod = READING_RESULTS_PERIOD_MIN;
		REPORT(RRReportInterval report(INF3,"Detecting direct illumination...\n"));
		priv->detectedCustomRGBA8 = detectDirectIllumination();
		if(!priv->detectedCustomRGBA8)
		{
			// detection has failed, ensure these points:
			// 1) detection will be detected again next time
			priv->dirtyLights = Private::BIG_CHANGE;
			// 2) eventual dirtyFactors = true; won't be forgotten
			// let normal dirtyFactors handler work now, exit later
			// 3) no calculations on current obsoleted primaries will be wasted
			// exit before resetting energies
			// exit before factor calculation and energy propagation
		}
	}
	if(dirtyFactors)
	{
		dirtyFactors = false;
		dirtyEnergies = Private::NO_CHANGE;
		priv->dirtyResults = true;
		REPORT(RRReportInterval report(INF3,"Resetting solver energies and factors...\n"));
		SAFE_DELETE(priv->packedSolver);
		if(priv->scene)
		{
			if(priv->detectedCustomRGBA8)
			{
				int numTriangles = (int)getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
				#pragma omp parallel for schedule(static)
				for(int t=0;t<numTriangles;t++)
				{
					unsigned color = priv->detectedCustomRGBA8[t];
					priv->multiObjectPhysicalWithIllumination->setTriangleIllumination(t,RM_IRRADIANCE_PHYSICAL,
						RRVec3(priv->customToPhysical[(color>>24)&255],priv->customToPhysical[(color>>16)&255],priv->customToPhysical[(color>>8)&255])
						);
				}
			}
			priv->scene->illuminationReset(true,true);
		}
		priv->solutionVersion++;
	}
	if(priv->dirtyLights!=Private::NO_CHANGE)
	{
		// exit in response to unsuccessful detectDirectIllumination
		return RRStaticSolver::NOT_IMPROVED;
	}
	if(dirtyEnergies!=Private::NO_CHANGE)
	{
		REPORT(RRReportInterval report(INF3,"Updating solver energies...\n"));
		if(priv->packedSolver)
		{
			priv->packedSolver->illuminationReset(priv->detectedCustomRGBA8,priv->customToPhysical);
		}
		else
		if(priv->scene)
		{
			if(priv->detectedCustomRGBA8)
			{
				int numTriangles = (int)getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
				#pragma omp parallel for schedule(static)
				for(int t=0;t<numTriangles;t++)
				{
					unsigned color = priv->detectedCustomRGBA8[t];
					priv->multiObjectPhysicalWithIllumination->setTriangleIllumination(t,RM_IRRADIANCE_PHYSICAL,
						RRVec3(priv->customToPhysical[(color>>24)&255],priv->customToPhysical[(color>>16)&255],priv->customToPhysical[(color>>8)&255])
						);
				}
			}
			priv->scene->illuminationReset(false,dirtyEnergies==Private::BIG_CHANGE);
		}
		priv->solutionVersion++;
		if(dirtyEnergies==Private::BIG_CHANGE)
		{
			// following improvement should be so big that single frames after big reset are not visibly darker
			// so...calculate at least 20ms?
			improveStep = MAX(improveStep,IMPROVE_STEP_MIN_AFTER_BIG_RESET);
		}
		dirtyEnergies = Private::NO_CHANGE;
		priv->dirtyResults = true;
	}

	REPORT(RRReportInterval report(INF3,"Calculating...\n"));
	TIME now = GETTIME;
	RRStaticSolver::Improvement improvement;
	if(priv->packedSolver)
	{
		priv->packedSolver->illuminationImprove(_params->qualityIndirectDynamic,_params->qualityIndirectStatic);
		improvement = RRStaticSolver::IMPROVED;
	}
	else
	{
		TIME end = (TIME)(now+improveStep*PER_SEC);
		improvement = priv->scene ? priv->scene->illuminationImprove(endByTime,(void*)&end) : RRStaticSolver::FINISHED;
	}
	//REPORT(RRReporter::report(INF3,"imp %d det+res+read %d game %d\n",(int)(1000*improveStep),(int)(1000*calcStep-improveStep),(int)(1000*userStep)));

	switch(improvement)
	{
		case RRStaticSolver::IMPROVED:
			priv->dirtyResults = true;
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

	if(priv->dirtyResults && now>=(TIME)(priv->lastReadingResultsTime+priv->readingResultsPeriod*PER_SEC))
	{
		priv->lastReadingResultsTime = now;
		if(priv->readingResultsPeriod<READING_RESULTS_PERIOD_MAX) priv->readingResultsPeriod *= READING_RESULTS_PERIOD_GROWTH;
		priv->dirtyResults = false;
		priv->solutionVersion++;
		improvement = RRStaticSolver::IMPROVED;
	}
	return improvement;
}

//#ifdef RR_DEVELOPMENT
void RRDynamicSolver::calculateReset(unsigned* detectedDirectIllumination)
{
	priv->packedSolver->illuminationReset(detectedDirectIllumination,priv->customToPhysical);
	priv->solutionVersion++;
}

void RRDynamicSolver::calculateImprove(CalculateParams* params)
{
	priv->packedSolver->illuminationImprove(params?params->qualityIndirectDynamic:CalculateParams().qualityIndirectDynamic,params?params->qualityIndirectStatic:CalculateParams().qualityIndirectStatic);
	priv->solutionVersion++;
}

void RRDynamicSolver::calculateUpdate()
{
	priv->packedSolver->getTriangleIrradianceIndirectUpdate();
}
//#endif

// adjusts timing, does no radiosity calculation (but calls calculateCore that does)
RRStaticSolver::Improvement RRDynamicSolver::calculate(CalculateParams* _params)
{
	TIME calcBeginTime = GETTIME;
	//printf("%f %f %f\n",calcBeginTime*1.0f,lastInteractionTime*1.0f,lastCalcEndTime*1.0f);

	// adjust userStep
	float lastUserStep = (float)((calcBeginTime-priv->lastCalcEndTime)/(float)PER_SEC);
	if(!lastUserStep) lastUserStep = 0.00001f; // fight with low timer precision, avoid 0, initial userStep=0 means 'unknown yet' which forces too long improve (IMPROVE_STEP_NO_INTERACTION)
	if(priv->lastInteractionTime && priv->lastInteractionTime>=priv->lastCalcEndTime)
	{
		// reportInteraction was called between this and previous calculate
		if(priv->lastCalcEndTime && lastUserStep<1.0f)
		{
			priv->userStep = lastUserStep;
		}
		//REPORT(RRReporter::report(INF1,"User %d ms.\n",(int)(1000*lastUserStep)));
	} else {
		// no reportInteraction was called between this and previous calculate
		// -> increase userStep
		//    (this slowly increases calculation time)
		priv->userStep = priv->lastInteractionTime ?
			(float)((priv->lastCalcEndTime-priv->lastInteractionTime)/(float)PER_SEC) // time from last interaction (if there was at least one)
			: IMPROVE_STEP_NO_INTERACTION; // safety time for situations there was no interaction yet
		//REPORT(RRReporter::report(INF1,"User %d ms (accumulated to %d).\n",(int)(1000*lastUserStep),(int)(1000*priv->userStep)));
	}

	// adjust improveStep
	if(!priv->userStep || !priv->calcStep || !priv->improveStep)
	{
		REPORT(RRReporter::report(INF1,"Reset to NO_INTERACT(%f,%f,%f).\n",priv->userStep,priv->calcStep,priv->improveStep));
		priv->improveStep = IMPROVE_STEP_NO_INTERACTION;
	}
	else
	{		
		// try to balance our (calculate) time and user time 1:1
		priv->improveStep *= 
			// kdyz byl userTime 1000 a nahle spadne na 1 (protoze user hmatnul na klavesy),
			// toto zajisti rychly pad improveStep z 80 na male hodnoty zajistujici plynuly render,
			// takze napr. rozjezd kamery nezacne strasnym cukanim
			(priv->calcStep>6*priv->userStep)?0.4f: 
			// standardni vyvazovani s tlumenim prudkych vykyvu
			( (priv->calcStep>priv->userStep)?0.8f:1.2f );
		// always spend at least 40% of our time in improve, don't spend too much by reading results etc
		priv->improveStep = MAX(priv->improveStep,MIN_PORTION_SPENT_IMPROVING*priv->calcStep);
		// stick in interactive bounds
		priv->improveStep = CLAMP(priv->improveStep,IMPROVE_STEP_MIN,IMPROVE_STEP_MAX);
	}

	// calculate
	RRStaticSolver::Improvement result = calculateCore(priv->improveStep,_params);

	// adjust calcStep
	priv->lastCalcEndTime = GETTIME;
	float lastCalcStep = (float)((priv->lastCalcEndTime-calcBeginTime)/(float)PER_SEC);
	if(!lastCalcStep) lastCalcStep = 0.00001f; // fight low timer precision, avoid 0, initial calcStep=0 means 'unknown yet'
	if(lastCalcStep<1.0)
	{
		if(!priv->calcStep)
			priv->calcStep = lastCalcStep;
		else
			priv->calcStep = 0.6f*priv->calcStep + 0.4f*lastCalcStep;
	}

	return result;
}

unsigned RRDynamicSolver::getSolutionVersion() const
{
	return priv->solutionVersion;
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
