#define SUPPORT_3DS
#define SUPPORT_BSP // Quake3
#define SUPPORT_DAE // Collada
#define SUPPORT_OBJ
#ifdef _WIN32
#define SUPPORT_MGF
#endif

// FCollada doesn't support VisualStudio 2003
#if defined(_MSC_VER) && (_MSC_VER < 1400)
#undef SUPPORT_DAE
#endif

#ifdef SUPPORT_DAE
	// must be included first because it collides with #define RR_SAFE_DELETE
	#include "FCollada.h"
	#include "FCDocument/FCDocument.h"
	#include "ImportCollada/RRObjectCollada.h"
#endif

#ifdef SUPPORT_3DS
	#include "Import3DS/Model_3DS.h"
	#include "Import3DS/RRObject3DS.h"
#endif

#ifdef SUPPORT_BSP
	#include "ImportQuake3/Q3Loader.h" // asi musi byt prvni, kvuli pragma pack
	#include "ImportQuake3/RRObjectBSP.h"
#endif

#ifdef SUPPORT_OBJ
	#include "ImportOBJ/RRObjectOBJ.h"
#endif

#ifdef SUPPORT_MGF
	#include "ImportMGF/RRObjectMGF.h"
#endif

#include "Lightsprint/IO/ImportScene.h"

using namespace rr_io;

#define MAX(a,b) (((a)>(b))?(a):(b))

/////////////////////////////////////////////////////////////////////////////
//
// Scene

ImportScene::ImportScene(const char* filename, float scale, bool stripPaths)
{
	rr::RRReportInterval report(rr::INF1,"Loading scene %s...\n",filename);
	objects = NULL;
	lights = NULL;

#ifdef SUPPORT_3DS
	// load .3ds scene
	scene_3ds = NULL;
	if(filename && strlen(filename)>=4 && _stricmp(filename+strlen(filename)-4,".3ds")==0)
	{
		scene_3ds = new Model_3DS;
		if(!scene_3ds->Load(filename,scale))
		{
			RR_SAFE_DELETE(scene_3ds);
		}
		else
		{
			objects = adaptObjectsFrom3DS(scene_3ds);
		}
	}
#endif

#ifdef SUPPORT_BSP
	// load quake 3 map
	scene_bsp = NULL;
	if(filename && strlen(filename)>=4 && _stricmp(filename+strlen(filename)-4,".bsp")==0)
	{
		scene_bsp = new TMapQ3;
		if(!readMap(filename,*scene_bsp))
		{
			RR_SAFE_DELETE(scene_bsp);
		}
		else
		{
			char* maps = _strdup(filename);
			char* mapsEnd;
			if(!stripPaths)
			{
				mapsEnd = MAX(strrchr(maps,'\\'),strrchr(maps,'/')); if(mapsEnd) mapsEnd[0] = 0;
			}
			mapsEnd = MAX(strrchr(maps,'\\'),strrchr(maps,'/')); if(mapsEnd) mapsEnd[1] = 0;
			objects = adaptObjectsFromTMapQ3(scene_bsp,maps,stripPaths,NULL);
			free(maps);
		}
	}
#endif

#ifdef SUPPORT_DAE
	// load collada document
	scene_dae = NULL;
	if(filename && strlen(filename)>=4 && _stricmp(filename+strlen(filename)-4,".dae")==0)
	{
		FCollada::Initialize();
		scene_dae = FCollada::NewTopDocument();
		FUErrorSimpleHandler errorHandler;
		FCollada::LoadDocumentFromFile(scene_dae,filename);
		if(!errorHandler.IsSuccessful())
		{
			rr::RRReporter::report(rr::ERRO,"%s\n",errorHandler.GetErrorString());
			RR_SAFE_DELETE(scene_dae);
		}
		else
		{
			char* pathToFile = _strdup(filename);
			if(stripPaths)
			{
				char* tmp = MAX(strrchr(pathToFile,'\\'),strrchr(pathToFile,'/'));
				if(tmp) tmp[1] = 0;
			}			
			rr::RRReportInterval report(rr::INF3,"Adapting scene...\n");
			objects = adaptObjectsFromFCollada(scene_dae,stripPaths?pathToFile:"",stripPaths);
			lights = adaptLightsFromFCollada(scene_dae);
			free(pathToFile);
		}
	}
#endif

#ifdef SUPPORT_OBJ
	// load obj scene
	if(filename && strlen(filename)>=4 && _stricmp(filename+strlen(filename)-4,".obj")==0)
	{
		objects = adaptObjectsFromOBJ(filename,scale);
	}
#endif

#ifdef SUPPORT_MGF
	// load mgf scene
	if(filename && strlen(filename)>=4 && _stricmp(filename+strlen(filename)-4,".mgf")==0)
	{
		objects = adaptObjectsFromMGF(filename);
	}
#endif

	if((!objects || !objects->size()) && !lights)
	{
		rr::RRReporter::report(rr::ERRO,"Scene %s not loaded.\n",filename);
	}
}

ImportScene::~ImportScene()
{
	delete lights;
	delete objects;

#ifdef SUPPORT_3DS
	delete scene_3ds;
#endif

#ifdef SUPPORT_BSP
	if(scene_bsp) freeMap(*scene_bsp);
	delete scene_bsp;
#endif

#ifdef SUPPORT_DAE
	SAFE_RELEASE(scene_dae);
	FCollada::Release();
#endif
}
