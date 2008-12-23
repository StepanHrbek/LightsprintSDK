// --------------------------------------------------------------------------
// BuildLightmaps command-line tool
//
// This is complex tool rather than simple sample.
// For much simpler lightmap build, see CPULightmaps sample.
//
// Usage
// - list of arguments is displayed when run without arguments
// - examples are provided in samples/BuildLightmaps/*.bat files
//
// This sample/tool builds various types of lighting
// - lightmaps
// - directional lightmaps
// - occlusion
// - bent normals
// - light detail maps
//
// It supports inputs in various file formats
// - Collada
// - Gamebryo
// - Quake 3
// - other partially supported formats
//
// It supports outputs in various file formats
// - png
// - jpg
// - tga
// - hdr (floats)
// - exr (floats)
// - bmp
// - jp2
// - and more
//
// It supports building
// - global illumination
// - direct illumination only
// - indirect illumination only
//
// It supports building
// - per-pixel illumination (maps)
// - per-vertex illumination (vertex buffers)
//
// Nearly all parameters may be set differently for different objects.
//
// Quality of produced maps highly depends on unwrap provided with scene.
// If we don't find unwrap in scene file, we build low buildQuality unwrap automatically,
// just to show something, even though results are poor.
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007-2008
// --------------------------------------------------------------------------

#define SCENE_VIEWER // adds viewer option, runs scene viewer after lightmap build

#include <cstdio>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <direct.h> // _chdir
#endif
#include "Lightsprint/RRMath.h"
#include "Lightsprint/IO/ImportScene.h"
#ifdef SCENE_VIEWER
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/Texture.h"
#endif


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, const char* caption = "Houston, we have a problem")
{
#ifdef _WIN32
	MessageBox(NULL,message,caption,MB_OK);
#else
	printf("%s: %s\n\nHit enter to close...",caption,message);
	fgetc(stdin);
#endif
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// supported layers

enum 
{
	LAYER_LIGHTMAP = 0,
	LAYER_OCCLUSION,
	LAYER_DIRECTIONAL1,
	LAYER_DIRECTIONAL2,
	LAYER_DIRECTIONAL3,
	LAYER_BENT_NORMALS,
	LAYER_LAST
};


/////////////////////////////////////////////////////////////////////////////
//
// commandline parsing

struct Parameters
{
	// global
	char* sceneFilename;
	unsigned buildQuality;
	bool buildDirect;
	bool buildIndirect;
	bool runViewer;

	// per object
	bool buildDirectional;
	bool buildOcclusion;
	bool buildBentNormals;
	rr::RRObjects::LayerParameters layerParameters;

	// create parameters from commandline arguments
	Parameters(int argc, char **argv, int objectIndex=-1)
	{
		// set defaults
		sceneFilename = NULL;
		buildQuality = 0;
		buildDirect = true;
		buildIndirect = true;
		buildDirectional = false;
		buildOcclusion = false;
		buildBentNormals = false;
		runViewer = false;
		layerParameters.objectIndex = objectIndex;

		// parse commandline
		int parsingObjectIndex = -1;
		for (int i=1;i<argc;i++)
		{
			if (sscanf(argv[i],"object:%d",&parsingObjectIndex)==1)
			{
			}
			else
			if (parsingObjectIndex==-1 || parsingObjectIndex==objectIndex)
			{
				if (sscanf(argv[i],"quality=%d",&buildQuality)==1)
				{
				}
				else
				if (!strcmp(argv[i],"direct"))
				{
					buildDirect = true;
				}
				else
				if (!strcmp(argv[i],"indirect"))
				{
					buildIndirect = true;
				}
				else
				if (!strcmp(argv[i],"occlusion"))
				{
					buildOcclusion = true;
				}
				else
				if (!strcmp(argv[i],"directional"))
				{
					buildDirectional = true;
				}
				else
				if (!strcmp(argv[i],"bentnormals"))
				{
					buildBentNormals = true;
				}
				else
				if (sscanf(argv[i],"mapsize=%d",&layerParameters.suggestedMapSize)==1)
				{
				}
				else
				if (sscanf(argv[i],"minmapsize=%d",&layerParameters.suggestedMinMapSize)==1)
				{
				}
				else
				if (sscanf(argv[i],"maxmapsize=%d",&layerParameters.suggestedMaxMapSize)==1)
				{
				}
				else
				if (sscanf(argv[i],"pixelsperworldunit=%f",&layerParameters.suggestedPixelsPerWorldUnit)==1)
				{
				}
				else
				if (!strcmp(argv[i],"viewer"))
				{
					runViewer = true;
				}
				else
				if (!strncmp(argv[i],"outputpath=",11))
				{
					layerParameters.suggestedPath = argv[i]+11;
				}
				else
				if (!strncmp(argv[i],"outputext=",10))
				{
					layerParameters.suggestedExt = argv[i]+10;
				}
				else
				{
					FILE* f = fopen(argv[i],"rb");
					if (f)
					{
						sceneFilename = argv[i];
						fclose(f);
					}
					else
					{
						rr::RRReporter::report(rr::WARN,"Unknown argument or file not found: %s\n",argv[i]);
					}
				}
			}
		}
		if (!buildDirect && !buildIndirect)
		{
			buildDirect = true;
			buildIndirect = true;
		}
	}

	// allocate layers for 1 object (lightmaps etc)
	void layersCreate(const rr::RRIlluminatedObject* illuminatedObject) const
	{
		illuminatedObject->illumination->getLayer(LAYER_LIGHTMAP)     = !buildOcclusion  ? layerParameters.createBuffer() : NULL;
		illuminatedObject->illumination->getLayer(LAYER_OCCLUSION)    = buildOcclusion   ? layerParameters.createBuffer() : NULL;
		illuminatedObject->illumination->getLayer(LAYER_DIRECTIONAL1) = buildDirectional ? layerParameters.createBuffer() : NULL;
		illuminatedObject->illumination->getLayer(LAYER_DIRECTIONAL2) = buildDirectional ? layerParameters.createBuffer() : NULL;
		illuminatedObject->illumination->getLayer(LAYER_DIRECTIONAL3) = buildDirectional ? layerParameters.createBuffer() : NULL;
		illuminatedObject->illumination->getLayer(LAYER_BENT_NORMALS) = buildBentNormals ? layerParameters.createBuffer() : NULL;
	}

	// save layers of 1 object, returns number of successfully saved layers
	unsigned layersSave(const rr::RRIlluminatedObject* illuminatedObject) const
	{
		unsigned saved = 0;
		for (unsigned layerIndex = LAYER_LIGHTMAP; layerIndex<LAYER_LAST; layerIndex++)
		{
			if (illuminatedObject->illumination->getLayer(layerIndex))
			{
				// insert layer name before extension
				std::string filename = layerParameters.actualFilename;
				const char* layerName[] = {"","occlusion.","directional1.","directional2.","directional3.","bentnormals."};
				int ofs = (int)filename.rfind('.',-1);
				if (ofs>=0) filename.insert(ofs+1,layerName[layerIndex]);
				// save
				saved += illuminatedObject->illumination->getLayer(layerIndex)->save(filename.c_str());
			}
		}
		return saved;
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
#ifdef _MSC_VER
	// check that we don't leak memory
	//_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 70;
#endif

	//
	// check for dll version mismatch
	//
	if (!RR_INTERFACE_OK)
	{
		error("DLL version mismatch.");
	}

	//
	// create message reporter
	//
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
#ifdef _WIN32
	rr::RRReporter::setReporter(rr::RRReporter::createWindowedReporter(solver));
#else
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
#endif

	//
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	//
#ifdef _WIN32
	char* exedir = _strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	_chdir(exedir);
	free(exedir);
#endif

	//
	// start with defaults
	//
	Parameters globalParameters(argc,argv);
	if (!globalParameters.sceneFilename)
	{
		error(
			"Lightsprint commandline lightmap builder\n"
			"\n"
			"Usage:\n"
			"  BuildLightmaps [global args] [default object args] [object:n [n-th object args]]\n"
			"\n"
			"Global arguments:\n"
			"  scene                   (filename of scene in supported format)\n"
			"  quality=100             (10=low, 100=medium, 1000=high)\n"
			"  direct                  (build direct lighting only)\n"
			"  indirect                (build indirect lighting only)\n"
			"  occlusion               (build ambient occlusion instead of lightmaps)\n"
#ifdef SCENE_VIEWER
			"  viewer                  (run scene viewer after build)"
#endif
			"\n"
			"Per-object arguments:\n"
			"  directional             (build directional lightmaps)\n"
			"  bentnormals             (build bent normals)\n"
			"  outputpath=\"where/to/save/lightmaps/\"\n"
			"  outputext=png           (format of saved maps, jpg, tga, hdr, png, bmp...)\n"
			"  mapsize=256             (map resolution, 0=build vertex buffers)\n"
			"  minmapsize=32           (minimal map resolution, Gamebryo only)\n"
			"  maxmapsize=1024         (maximal map resolution, Gamebryo only)\n"
			"  pixelsperworldunit=1.0  (Gamebryo only)\n"
			,
			"Scene filename not set");
	}

	//
	// load licence
	//
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
	{
		error(licError);
	}

	//
	// init image loaders
	//
	rr_io::setImageLoader();

	//
	// switch inputs and outputs from HDR physical scale to sRGB screenspace
	//
	solver->setScaler(rr::RRScaler::createRgbScaler());

	//
	// load scene
	//
	rr_io::ImportScene scene(globalParameters.sceneFilename);

	//
	// set solver geometry
	//
	if (scene.getObjects())
	{
		solver->setStaticObjects(*scene.getObjects(), NULL);
	}

	//
	// set solver lights
	//
	if (globalParameters.buildOcclusion)
	{
		solver->setEnvironment( rr::RRBuffer::createSky() );
	}
	else
	{
		if (scene.getLights())
		{
			solver->setLights(*scene.getLights());
		}
	}

	//
	// allocate layers (decide resolution, format)
	//
	if (scene.getObjects())
	{
		for (unsigned objectIndex=0;objectIndex<scene.getObjects()->size();objectIndex++)
		{
			// take per-object parameters
			Parameters objectParameters(argc,argv,objectIndex);
			// query size, format etc
			scene.getObjects()->recommendLayerParameters(objectParameters.layerParameters);
			// allocate
			objectParameters.layersCreate(&(*scene.getObjects())[objectIndex]);
		}
	}

	//
	// decrease priority, so that this task runs on background using only free CPU cycles
	//
#ifdef _WIN32
	SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#endif

	//
	// build lighting in layers
	//
	if (globalParameters.buildQuality)
	{
		rr::RRReportInterval report(rr::INF1,"Building lighting...\n");

		rr::RRDynamicSolver::UpdateParameters params(globalParameters.buildQuality);
		solver->updateLightmaps(
			globalParameters.buildOcclusion ? LAYER_OCCLUSION : LAYER_LIGHTMAP,
			LAYER_DIRECTIONAL1,
			LAYER_BENT_NORMALS,
			globalParameters.buildDirect ? &params : NULL,
			globalParameters.buildIndirect ? &params : NULL,
			NULL);
	}

	//
	// restore priority
	//
#ifdef _WIN32
	SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
#endif

	//
	// save layers
	//
	if (globalParameters.buildQuality && scene.getObjects())
	{
		for (unsigned objectIndex=0;objectIndex<scene.getObjects()->size();objectIndex++)
		{
			// take per-object parameters
			Parameters objectParameters(argc,argv,objectIndex);
			// query filename
			scene.getObjects()->recommendLayerParameters(objectParameters.layerParameters);
			// save
			objectParameters.layersSave(&(*scene.getObjects())[objectIndex]);
		}
	}

	//
	// run interactive scene viewer
	//
#ifdef SCENE_VIEWER
	if (globalParameters.runViewer || !globalParameters.buildQuality)
	{
		rr_gl::SceneViewerState svs;
		svs.adjustTonemapping = false;
		svs.renderHelpers = true;
		svs.renderRealtime = globalParameters.buildQuality==0; // switch from default realtime GI to static GI
		svs.staticLayerNumber = globalParameters.buildOcclusion ? LAYER_OCCLUSION : LAYER_LIGHTMAP; // switch from default layer to our layer 0
		rr_gl::sceneViewer(solver,"../../data/shaders/",&svs);
		rr_gl::deleteAllTextures();
	}
#endif

	//
	// free memory
	//
	delete solver->getEnvironment();
	delete solver->getScaler();
	RR_SAFE_DELETE(solver); // sets solver to NULL (it's important because reporter still references solver)
	delete rr::RRReporter::getReporter(); // deletes current reporter, current is set to NULL

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
// WinMain() calls main() for compatibility with Linux

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
	int argc;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	char** argv = new char*[argc+1];
	for (int i=0;i<argc;i++)
	{
		argv[i] = (char*)malloc(wcslen(argvw[i])+1);
		sprintf(argv[i], "%ws", argvw[i]);
	}
	argv[argc] = NULL;
	return main(argc,argv);
}
#endif
