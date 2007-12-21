#ifndef LEVEL_H
#define LEVEL_H

#define SUPPORT_3DS
#define SUPPORT_COLLADA
#define SUPPORT_BSP
#define SUPPORT_MGF
//#define SUPPORT_UE3

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
	#include "../../samples/Import3DS/Model_3DS.h"
	#include "../../samples/Import3DS/RRObject3DS.h"
#endif

#ifdef SUPPORT_MGF
	#include "../../samples/ImportMGF/RRObjectMGF.h"
#endif

#ifdef SUPPORT_UE3
	#include "../../samples/ImportUE3/RRObjectUE3.h"
#endif

#include "AnimationEditor.h"
#include "Autopilot.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"


/////////////////////////////////////////////////////////////////////////////
//
// Level

class Level
{
public:
	Level(LevelSetup* levelSetup, rr::RRBuffer* skyMap, bool supportEditor);
	~Level();

	Autopilot pilot;
	AnimationEditor* animationEditor;

	enum Type
	{
		TYPE_3DS = 0,
		TYPE_BSP,
		TYPE_DAE,
		TYPE_MGF,
		TYPE_NONE,
	};
	Type type;
#ifdef SUPPORT_3DS
	Model_3DS m3ds;
#endif
#ifdef SUPPORT_BSP
	TMapQ3 bsp;
#endif
#ifdef SUPPORT_COLLADA
	FCDocument* collada;
#endif
	rr::RRObjects* objects; // objects adapted from native format
	rr_gl::RRDynamicSolverGL* solver;
	class Bugs* bugs;
	rr_gl::RendererOfScene* rendererOfScene;

	unsigned saveIllumination(const char* path, bool vertexColors, bool lightmaps);
	unsigned loadIllumination(const char* path, bool vertexColors, bool lightmaps);
};

#endif
