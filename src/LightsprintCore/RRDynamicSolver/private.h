#ifndef PRIVATE_H
#define PRIVATE_H

#include <vector>
#include "Lightsprint/RRDynamicSolver.h"
#include "../RRStaticSolver/RRPackedSolver.h"

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
		RRPackedSolver* packedSolver;
		// read results
		RRScaler*  scaler;
		struct TriangleVertexPair {unsigned triangleIndex:30;unsigned vertex012:2;TriangleVertexPair(unsigned _triangleIndex,unsigned _vertex012):triangleIndex(_triangleIndex),vertex012(_vertex012){}}; // packed as 30+2 bits is much faster than 32+32 bits
		std::vector<std::vector<TriangleVertexPair> > preVertex2PostTriangleVertex; ///< readResults lookup table for RRDynamicSolver
		std::vector<std::vector<const RRVec3*> > preVertex2Ivertex; ///< readResults lookup table for RRPackedSolver

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
			//preVertex2Ivertex zeroed by constructor
			packedSolver = NULL;
		}
		~Private()
		{
			delete packedSolver;
			delete scene;
			delete multiObjectPhysicalWithIllumination;
			if(multiObjectPhysical!=multiObjectCustom) delete multiObjectPhysical; // no scaler -> physical == custom
			delete multiObjectCustom;
		}
	};

} // namespace

#endif
