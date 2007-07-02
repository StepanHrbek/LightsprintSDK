#ifndef PRIVATE_H
#define PRIVATE_H

#include <vector>
#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

	struct RRDynamicSolver::Private
	{
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};
		// calculate
		RRObjects  objects;
		RRLights   lights;
		const RRIlluminationEnvironmentMap* environment;
		RRStaticSolver::SmoothingParameters smoothing;
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		ChangeStrength dirtyLights; // 0=no light change, 1=small light change, 2=strong light change
		bool       dirtyResults;
		long       lastInteractionTime;
		long       lastCalcEndTime;
		long       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRObject*  multiObjectCustom;
		RRObjectWithPhysicalMaterials* multiObjectPhysical;
		RRObjectWithIllumination* multiObjectPhysicalWithIllumination;
		RRStaticSolver*   scene;
		unsigned   solutionVersion;
		RRReal     minimalSafeDistance; // minimal distance safely used in current scene, proportional to scene size
		// read results
		RRScaler*  scaler;
		std::vector<std::vector<std::pair<unsigned,unsigned> > > preVertex2PostTriangleVertex; ///< readResults lookup table

		Private()
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
			minimalSafeDistance = 0;
			//preVertex2PostTriangleVertex zeroed by constructor
		}
		~Private()
		{
			delete scene;
			delete multiObjectPhysicalWithIllumination;
			if(multiObjectPhysical!=multiObjectCustom) delete multiObjectPhysical; // no scaler -> physical == custom
			delete multiObjectCustom;
		}
	};

} // namespace

#endif
