#ifndef LEVEL_H
#define LEVEL_H

#define SUPPORT_3DS
#define SUPPORT_COLLADA
#define SUPPORT_BSP

#ifdef SUPPORT_COLLADA
	#include "FCollada.h"
	#include "FCDocument/FCDocument.h"
	#include "../../samples/ImportCollada/RRObjectCollada.h"
#endif

#ifdef SUPPORT_BSP
	#include "../../samples/ImportQuake3/Q3Loader.h" // asi musi byt prvni, kvuli pragma pack
	#include "../../samples/ImportQUake3/RRObjectBSP.h"
#endif

#ifdef SUPPORT_3DS
	#include "../../samples/Import3DS/RRObject3DS.h"
#endif

#include "AnimationEditor.h"
#include "Autopilot.h"
#include "Lightsprint/DemoEngine/RendererWithCache.h"
#include "Lightsprint/RRDynamicSolver.h"


/////////////////////////////////////////////////////////////////////////////
//
// Level

class Level
{
public:
	Level(LevelSetup* levelSetup, rr::RRIlluminationEnvironmentMap* skyMap, bool supportEditor);
	~Level();

	Autopilot pilot;
	AnimationEditor* animationEditor;

	enum Type
	{
		TYPE_3DS = 0,
		TYPE_BSP,
		TYPE_DAE,
		TYPE_NONE,
	};
	Type type;
#ifdef SUPPORT_3DS
	de::Model_3DS m3ds;
#endif
#ifdef SUPPORT_BSP
	de::TMapQ3 bsp;
#endif
#ifdef SUPPORT_COLLADA
	FCDocument* collada;
#endif
	rr::RRObjects* objects; // objects adapted from native format
	rr::RRDynamicSolver* solver;
	class Bugs* bugs;
	rr_gl::RendererOfRRObject* rendererNonCaching;
	de::RendererWithCache* rendererCaching;
};

#endif
