// --------------------------------------------------------------------------
// BuildLightmaps command-line tool
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
// Map quality highly depends on unwrap provided with scene.
// If we don't find unwrap in scene file, we build low quality unwrap automatically,
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
// main

int main(int argc, char **argv)
{
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
	char* sceneFilename = NULL;
	const char* outputPath;
	const char* outputExt = "png";
	unsigned quality = 0;
	unsigned mapSize = 256;
	unsigned mapSizeMin = 32;
	unsigned mapSizeMax = 1024;
	float pixelsPerWorldUnit = 1;
	bool buildDirect = true;
	bool buildIndirect = true;
	bool buildDirectional = false;
	bool buildOcclusion = false;
	bool buildBentNormals = false;
	//bool buildLDM = false;
	bool runViewer = false;

	//
	// parse commandline
	//
	for (int i=1;i<argc;i++)
	{
		if (sscanf(argv[i],"quality=%d",&quality)==1)
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
		//if (!strcmp(argv[i],"ldm"))
		//{
		//	buildLDM = true;
		//}
		//else
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
		if (sscanf(argv[i],"mapsize=%d",&mapSize)==1)
		{
		}
		else
		if (sscanf(argv[i],"minmapsize=%d",&mapSizeMin)==1)
		{
		}
		else
		if (sscanf(argv[i],"maxmapsize=%d",&mapSizeMax)==1)
		{
		}
		else
		if (sscanf(argv[i],"pixelsperworldunit=%f",&pixelsPerWorldUnit)==1)
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
			outputPath = argv[i]+11;
		}
		else
		if (!strncmp(argv[i],"outputext=",10))
		{
			outputExt = argv[i]+10;
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
	if (!sceneFilename)
	{
		error(
			"Lightsprint commandline lightmap builder\n"
			"\n"
			"Usage:\n"
			"  BuildLightmaps scene [arguments]\n"
			"\n"
			"Optional arguments:\n"
			"  quality=100             (10=low, 100=medium, 1000=high)\n"
			"  direct                  (build direct lighting only)\n"
			"  indirect                (build indirect lighting only)\n"
			"  directional             (build directional lightmaps)\n"
			"  occlusion               (build occlusion)\n"
			"  bentnormals             (build bent normals)\n"
			//"  ldm                     (build light detail maps)\n"
			"  outputpath=\"where/to/save/lightmaps/\"\n"
			"  outputext=png           (format of saved maps, jpg, tga, hdr, png, bmp...)\n"
			"  mapsize=256             (map resolution, 0=build vertex buffers)\n"
			"  minmapsize=32           (minimal map resolution, Gamebryo only)\n"
			"  maxmapsize=1024         (maximal map resolution, Gamebryo only)\n"
			"  pixelsperworldunit=1.0  (Gamebryo only)\n"
#ifdef SCENE_VIEWER
			"  viewer                  (run scene viewer after build)"
#endif
			,
			"Scene filename not set");
	}
	if (!buildDirect && !buildIndirect)
	{
		buildDirect = true;
		buildIndirect = true;
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
	rr_io::ImportScene scene(sceneFilename);

	//
	// set solver geometry
	//
	if (scene.getObjects())
	{
		solver->setStaticObjects(*scene.getObjects(), NULL,NULL,rr::RRCollider::IT_BSP_COMPACT);//!!!
	}

	//
	// set solver lights
	//
	if (buildOcclusion)
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
	// assign numbers to requested layers of information
	//
	int layerLightmaps = buildOcclusion ? -1 : 0;
	int layerOcclusion = buildOcclusion ? 0 : -1;
	int layerDirectional = buildDirectional ? 1 : -3;
	int layerBentNormals = buildBentNormals ? 4 : -1;
	//int layerLDM = buildLDM ? 5 : -1;

	//
	// allocate layers
	//
	if (scene.getObjects())
	{
		rr::RRIlluminatedObject::LayerParameters params(mapSize);
		params.mapSizeMin = mapSizeMin;
		params.mapSizeMax = mapSizeMax;
		params.pixelsPerWorldUnit = pixelsPerWorldUnit;
		scene.getObjects()->createLayer(layerLightmaps,params);
		scene.getObjects()->createLayer(layerOcclusion,params);
		scene.getObjects()->createLayer(layerDirectional,params);
		scene.getObjects()->createLayer(layerDirectional+1,params);
		scene.getObjects()->createLayer(layerDirectional+2,params);
		scene.getObjects()->createLayer(layerBentNormals,params);
		//scene.getObjects()->createLayer(layerLDM,params);
	}

	//
	// decrease priority, so that this task runs on background using only free CPU cycles
	//
#ifdef _WIN32
	SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#endif

	//
	// build layers
	//
	if (quality)
	{
		rr::RRReportInterval report(rr::INF1,"Building lighting...\n");

		rr::RRDynamicSolver::UpdateParameters params(quality);
		solver->updateLightmaps(
			(layerLightmaps>=0)?layerLightmaps:layerOcclusion,
			layerDirectional,
			layerBentNormals,
			buildDirect ? &params : NULL,
			buildIndirect ? &params : NULL,
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
	if (quality && scene.getObjects())
	{
		scene.getObjects()->saveLayer(layerLightmaps    ,outputPath,(std::string(""             )+outputExt).c_str());
		scene.getObjects()->saveLayer(layerDirectional  ,outputPath,(std::string("directional1.")+outputExt).c_str());
		scene.getObjects()->saveLayer(layerDirectional+1,outputPath,(std::string("directional2.")+outputExt).c_str());
		scene.getObjects()->saveLayer(layerDirectional+2,outputPath,(std::string("directional3.")+outputExt).c_str());
		scene.getObjects()->saveLayer(layerOcclusion    ,outputPath,(std::string("occlusion."   )+outputExt).c_str());
		scene.getObjects()->saveLayer(layerBentNormals  ,outputPath,(std::string("bentnormals." )+outputExt).c_str());
		//scene.getObjects()->saveLayer(layerLDM          ,(outputPath,(std::string("ldm."         )+outputExt).c_str());
	}

	//
	// run interactive scene viewer
	//
#ifdef SCENE_VIEWER
	if (runViewer || !quality)
	{
		rr_gl::SceneViewerState svs;
		svs.adjustTonemapping = false;
		svs.renderHelpers = true;
		svs.renderRealtime = quality==0; // switch from default realtime GI to static GI
		svs.staticLayerNumber = 0; // switch from default layer to our layer 0
		rr_gl::sceneViewer(solver,true,"../../data/shaders/",&svs);
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
// WinMain for compatibility with Linux

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
