// --------------------------------------------------------------------------
// BuildLightmaps command-line tool
//
// This is real tool rather than simple sample.
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
// - other partially supported formats
//
// It supports outputs in various file formats
// - png
// - jpg
// - tga
// - hdr (floats)
// - exr (floats)
// - bmp
// - jp2 (jpeg2000)
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
// It supports skylight
// - uniform sky color
// - upper and lower hemisphere colors
// - 1 skybox texture (may be hdr) (equirectangular or 3:4 or 4:3 cross)
// - 6 skybox textures (may be hdr)
// - custom: emissive sky mesh in scene
//
// Many parameters may be set differently for different objects.
//
// Quality of produced maps highly depends on quality of unwrap.
// Unwrap is imported from scene file.
// It can also be generated by RRObjects::buildUnwrap(), but we don't do it in this tool.
//
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#define SCENE_VIEWER // adds viewer option, runs scene viewer after lightmap build

#include <cstdio>
#include <string>
#include <vector>
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

void error(bool& aborting, const char* message)
{
	if (message)
		rr::RRReporter::report(rr::ERRO,"%s\n",message);
#ifdef _WIN32
	rr::RRReporter::report(rr::INF1,"\nPress Esc or click Abort to quit...\n");
	while (!aborting) Sleep(1);
#else
	printf("\nHit Enter to close...");
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
	rr::RRVec4 skyUpper;
	rr::RRVec4 skyLower;
	const char* skyBox;
	float emissiveMultiplier;
	unsigned buildQuality;
	float directLightMultiplier;
	float indirectLightMultiplier;
	bool runViewer;

	// per object
	bool buildDirectional;
	bool buildOcclusion;
	bool buildBentNormals;
	bool buildNothing;
	rr::RRObject::LayerParameters layerParameters;
	float aoIntensity;
	float aoSize;
	float ppSmoothing;
	float ppBrightness;
	float ppContrast;

	// create parameters from commandline arguments
	//  objectIndex=-1 -> gather global parameters
	//  objectIndex=n -> gather parameters for object n
	Parameters(int argc, char** argv, int objectIndex=-1)
	{
		// set defaults
		sceneFilename = NULL;
		skyUpper = rr::RRVec4(0);
		skyLower = rr::RRVec4(0);
		skyBox = NULL;
		emissiveMultiplier = 1;
		buildQuality = 0;
		directLightMultiplier = 1;
		indirectLightMultiplier = 1;
		buildDirectional = false;
		buildOcclusion = false;
		buildBentNormals = false;
		buildNothing = false;
		runViewer = false;
		layerParameters.objectIndex = objectIndex;
		aoIntensity = 1;
		aoSize = 0;
		ppSmoothing = 0;
		ppBrightness = 1;
		ppContrast = 1;

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
				if (!strcmp(argv[i],"nothing"))
				{
					buildNothing = true;
				}
				else
				if (sscanf(argv[i],"skyupper=%f;%f;%f",&skyUpper.x,&skyUpper.y,&skyUpper.z)==3)
				{
				}
				else
				if (sscanf(argv[i],"skylower=%f;%f;%f",&skyLower.x,&skyLower.y,&skyLower.z)==3)
				{
				}
				else
				if (sscanf(argv[i],"skycolor=%f;%f;%f",&skyUpper.x,&skyUpper.y,&skyUpper.z)==3)
				{
					skyLower = skyUpper;
				}
				else
				if (!strncmp(argv[i],"skybox=",7))
				{
					skyBox = argv[i]+7;
				}
				else
				if (sscanf(argv[i],"directmultiplier=%f",&directLightMultiplier)==1)
				{
				}
				else
				if (sscanf(argv[i],"indirectmultiplier=%f",&indirectLightMultiplier)==1)
				{
				}
				else
				if (sscanf(argv[i],"emissivemultiplier=%f",&emissiveMultiplier)==1)
				{
				}
				else
				if (sscanf(argv[i],"mapsize=%d*%d",&layerParameters.suggestedMapWidth,&layerParameters.suggestedMapHeight)==2)
				{
				}
				else
				if (sscanf(argv[i],"mapsize=%d",&layerParameters.suggestedMapWidth)==1)
				{
					layerParameters.suggestedMapHeight = layerParameters.suggestedMapWidth;
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
				if (sscanf(argv[i],"aointensity=%f",&aoIntensity)==1)
				{
				}
				else
				if (sscanf(argv[i],"aosize=%f",&aoSize)==1)
				{
				}
				else
				if (sscanf(argv[i],"smoothing=%f",&ppSmoothing)==1)
				{
				}
				else
				if (sscanf(argv[i],"brightness=%f",&ppBrightness)==1)
				{
				}
				else
				if (sscanf(argv[i],"contrast=%f",&ppContrast)==1)
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
				if (!strncmp(argv[i],"outputname=",11))
				{
					layerParameters.suggestedName = argv[i]+11;
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
					if (objectIndex<0 || parsingObjectIndex>=0) // skip errors in global args in non-global pass (to avoid reporting it many times)
					{
						rr::RRReporter::report(rr::WARN,"Unknown argument or file not found: %s\n",argv[i]);
					}
				}
			}
		}
		if (buildNothing)
		{
			buildOcclusion = false;
			buildDirectional = false;
			buildBentNormals = false;
		}
		// outputpath is relative to currentdirectory=exedirectory
		// TODO: make it relative to scenepath
		//  outputpath not set -> outputpath=scenepath
		//  outputpath= -> outputpath=scenepath
		//  outputpath=. -> outputpath=scenepath/.
		//  outputpath=aga -> outputpath=scenepath/aga
	}

	rr::RRBuffer* newBuffer() const
	{
		return layerParameters.createBuffer(false,true); // floats not necessary, alpha for smoothing
	}

	// allocate layers for 1 object (lightmaps etc)
	void layersCreate(rr::RRObjectIllumination* illumination) const
	{
		if (!buildNothing)
		{
			illumination->getLayer(LAYER_LIGHTMAP)     = !buildOcclusion  ? newBuffer() : NULL;
			illumination->getLayer(LAYER_OCCLUSION)    = buildOcclusion   ? newBuffer() : NULL;
			illumination->getLayer(LAYER_DIRECTIONAL1) = buildDirectional ? newBuffer() : NULL;
			illumination->getLayer(LAYER_DIRECTIONAL2) = buildDirectional ? newBuffer() : NULL;
			illumination->getLayer(LAYER_DIRECTIONAL3) = buildDirectional ? newBuffer() : NULL;
			illumination->getLayer(LAYER_BENT_NORMALS) = buildBentNormals ? newBuffer() : NULL;
		}
	}

	// postprocess layers of 1 object, returns number of successfully saved layers
	unsigned layersPostprocess(const rr::RRObjectIllumination* illumination) const
	{
		unsigned postprocessed = 0;
		for (unsigned layerIndex = LAYER_LIGHTMAP; layerIndex<LAYER_LAST; layerIndex++)
		{
			rr::RRBuffer* buffer = illumination->getLayer(layerIndex);
			if (buffer)
			{
				if (buffer->getType()==rr::BT_2D_TEXTURE)
				{
					buffer->lightmapSmooth(ppSmoothing,false);
					buffer->lightmapGrowForBilinearInterpolation(false);
					buffer->lightmapGrow(1024,false);
				}
				buffer->brightnessGamma(rr::RRVec4(ppBrightness),rr::RRVec4(ppContrast));
				postprocessed++;
			}
		}
		return postprocessed;
	}

	// save layers of 1 object, returns number of successfully saved layers
	unsigned layersSave(const rr::RRObjectIllumination* illumination) const
	{
		unsigned saved = 0;
		for (unsigned layerIndex = LAYER_LIGHTMAP; layerIndex<LAYER_LAST; layerIndex++)
		{
			if (illumination->getLayer(layerIndex))
			{
				// insert layer name before extension
				std::string filename = layerParameters.actualFilename.c_str();
				const char* layerName[] = {"","occlusion.","directional1.","directional2.","directional3.","bentnormals."};
				int ofs = (int)filename.rfind('.',-1);
				if (ofs>=0) filename.insert(ofs+1,layerName[layerIndex]);
				// save
				saved += illumination->getLayer(layerIndex)->save(filename.c_str());
			}
		}
		return saved;
	}

	void layersDelete(rr::RRObjectIllumination* illumination) const
	{
		for (unsigned layerIndex = LAYER_LIGHTMAP; layerIndex<LAYER_LAST; layerIndex++)
			RR_SAFE_DELETE(illumination->getLayer(layerIndex));
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char** argv)
{
#ifdef _MSC_VER
	// check that we don't leak memory
	//_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 70;
#endif

	//
	// create message reporter
	//
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
#ifdef _WIN32
	rr::RRReporter* reporter = rr::RRReporter::createWindowedReporter(solver,"BUILD LIGHTMAPS tool");
#else
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
#endif
	rr::RRReporter::report(rr::INF2,"Using Lightsprint SDK %s\n",rr::RR_INTERFACE_DESC_LIB());

	//
	// check for dll version mismatch
	//
	if (!RR_INTERFACE_OK)
	{
		error(solver->aborting,"DLL version mismatch.");
	}

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
	if (!globalParameters.sceneFilename && !globalParameters.runViewer)
	{
		rr::RRReporter::report(rr::INF1,"%s",
			"\n"
			"Lightsprint commandline lightmap builder\n"
			"\n"
			"Usage:\n"
			"  BuildLightmaps [global args] [default object args] [object:n [n-th object args]]\n"
			"\n"
			"Global arguments:\n"
			"  scene                   (filename of scene in supported format)\n"
			"  quality=100             (10=low, 100=medium, 1000=high)\n"
			"  occlusion               (build ambient occlusion instead of lightmaps)\n"
			"  skycolor=0.0;0.0;0.0    (color of both sky hemispheres)\n"
			"  skyupper=0.0;0.0;0.0    (color of upper sky hemisphere)\n"
			"  skylower=0.0;0.0;0.0    (color of lower sky hemisphere)\n"
			"  skybox=texture_filename (sky texture in LDR,HDR,cube,cross,equirect..)\n"
			"  directmultiplier=1.0    (multiplies direct effect of point/spot/dir)\n"
			"  indirectmultiplier=1.0  (multiplies indirect effect of point/spot/dir)\n"
			"  emissivemultiplier=1.0  (multiplies effect of emissive materials)\n"
#ifdef SCENE_VIEWER
			"  viewer                  (run scene viewer after build)\n"
#endif
			"\n"
			); // split in two messages, because standard reporters limit message to 1000 characters
		rr::RRReporter::report(rr::INF1,"%s",
			"Per-object arguments:\n"
			"  directional             (build directional lightmaps)\n"
			"  bentnormals             (build bent normals)\n"
			"  nothing                 (build nothing)\n"
			"  outputpath=\"where/to/save/lightmaps/\"\n"
			"  outputname=\"my_lightmap_name\"\n"
			"  outputext=png           (format of saved maps, jpg, tga, hdr, exr, bmp...)\n"
			"  mapsize=256*256         (map resolution, 0=build vertex buffers)\n"
			"  minmapsize=32           (minimal map resolution, Gamebryo only)\n"
			"  maxmapsize=1024         (maximal map resolution, Gamebryo only)\n"
			"  pixelsperworldunit=1.0  (Gamebryo only)\n"
			"  aointensity=1.0         (intensity of darkening in corners, 0=off, 2=high)\n"
			"  aosize=0.0              (how far from corners to darken, 0=off)\n"
			"  smoothing=0.0           (postprocess: smoothing, radius in pixels)\n"
			"  brightness=1.0          (postprocess: brightness adjustment)\n"
			"  contrast=1.0            (postprocess: contrast adjustment)\n"
			"\n"
			);
		error(solver->aborting,"Scene filename not set or not found.");
	}

	//
	// load licence
	//
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
	{
		error(solver->aborting,licError);
	}

	//
	// init image loaders
	//
	rr_io::registerLoaders();

	//
	// switch inputs and outputs from HDR physical scale to sRGB screenspace
	//
	solver->setScaler(rr::RRScaler::createRgbScaler());

	//
	// load scene
	//
	rr::RRScene scene(globalParameters.sceneFilename,NULL);
	if (!scene.objects.size() && !globalParameters.runViewer)
		error(solver->aborting,"No objects loaded.");
	scene.objects.multiplyEmittance(globalParameters.emissiveMultiplier);

	//
	// set solver geometry
	//
	solver->setStaticObjects(scene.objects, NULL);

	//
	// set solver lights
	//
	if (globalParameters.buildOcclusion)
	{
		solver->setEnvironment( rr::RRBuffer::createSky() );
	}
	else
	{
		solver->setLights(scene.lights);
		rr::RRBuffer* environment = NULL;
		if (globalParameters.skyBox)
		{
			environment = rr::RRBuffer::loadCube(globalParameters.skyBox);
			if (environment && globalParameters.skyUpper+globalParameters.skyLower!=rr::RRVec4(0))
				environment->multiplyAdd((globalParameters.skyUpper+globalParameters.skyLower)/2,rr::RRVec4(0));
		}
		else
		{
			if (!globalParameters.sceneFilename && globalParameters.skyUpper+globalParameters.skyLower==rr::RRVec4(0))
				// load some sky when running without scene and without sky
				environment = rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_up.jpg");
			else
			if (globalParameters.skyUpper+globalParameters.skyLower!=rr::RRVec4(0))
				environment = rr::RRBuffer::createSky(globalParameters.skyUpper,globalParameters.skyLower);
		}
		solver->setEnvironment(environment);
	}

	//
	// build and save lighting
	//
	if (globalParameters.buildQuality)
	{
		// apply indirect light multiplier
		std::vector<rr::RRVec3> lightColors;
		for (unsigned i=0;i<solver->getLights().size();i++)
		{
			lightColors.push_back(solver->getLights()[i]->color); // save original colors, just in case indirectLightMultiplier is 0
			solver->getLights()[i]->color *= globalParameters.indirectLightMultiplier;
		}

		// calculate indirect illumination in solver
		rr::RRDynamicSolver::UpdateParameters updateParameters(globalParameters.buildQuality);
		solver->updateLightmaps(-1,-1,-1,NULL,&updateParameters,NULL);
		updateParameters.applyCurrentSolution = true;
		updateParameters.aoIntensity = globalParameters.aoIntensity;
		updateParameters.aoSize = globalParameters.aoSize;

		// apply direct light multiplier
		for (unsigned i=0;i<solver->getLights().size();i++)
		{
			solver->getLights()[i]->color = lightColors[i]*globalParameters.directLightMultiplier;
		}

#ifdef _WIN32
		// decrease priority, so that this task runs on background using only free CPU cycles
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#endif

		// build and save vertex buffers (all at once, it's faster)
		unsigned saved = 0;
		if (!solver->aborting)
		{
			// allocate layers (decide resolution, format)
			for (unsigned objectIndex=0;objectIndex<scene.objects.size();objectIndex++)
			{
				// take per-object parameters
				Parameters objectParameters(argc,argv,objectIndex);
				// query size, format etc
				scene.objects[objectIndex]->recommendLayerParameters(objectParameters.layerParameters);
				// allocate
				if (objectParameters.layerParameters.actualType==rr::BT_VERTEX_BUFFER)
					objectParameters.layersCreate(&scene.objects[objectIndex]->illumination);
			}

			// build direct illumination
			solver->updateLightmaps(
				globalParameters.buildOcclusion ? LAYER_OCCLUSION : LAYER_LIGHTMAP,
				LAYER_DIRECTIONAL1,
				LAYER_BENT_NORMALS,
				&updateParameters,
				NULL,
				NULL);

			for (unsigned objectIndex=0;objectIndex<scene.objects.size();objectIndex++)
			if (!solver->aborting)
			{
				// take per-object parameters
				Parameters objectParameters(argc,argv,objectIndex);
				// query filename
				scene.objects[objectIndex]->recommendLayerParameters(objectParameters.layerParameters);
				// postprocess
				objectParameters.layersPostprocess(&scene.objects[objectIndex]->illumination);
				// save
				saved += objectParameters.layersSave(&scene.objects[objectIndex]->illumination);
				// free
				if (!globalParameters.runViewer)
					objectParameters.layersDelete(&scene.objects[objectIndex]->illumination);
			}
		}

		// build and save pixel buffers (one by one, to limit peak memory use)
		for (unsigned objectIndex=0;objectIndex<scene.objects.size();objectIndex++)
		if (!solver->aborting)
		{
			// allocate layers (decide resolution, format)
			// take per-object parameters
			Parameters objectParameters(argc,argv,objectIndex);
			// query size, format etc
			scene.objects[objectIndex]->recommendLayerParameters(objectParameters.layerParameters);
			if (objectParameters.layerParameters.actualType!=rr::BT_VERTEX_BUFFER)
			{
				rr::RRObjectIllumination& illumination = scene.objects[objectIndex]->illumination;
				// allocate
				objectParameters.layersCreate(&illumination);
				rr::RRBuffer* directionalBuffers[3];
				directionalBuffers[0] = illumination.getLayer(LAYER_DIRECTIONAL1);
				directionalBuffers[1] = illumination.getLayer(LAYER_DIRECTIONAL2);
				directionalBuffers[2] = illumination.getLayer(LAYER_DIRECTIONAL3);
				// build direct illumination
				solver->updateLightmap(objectIndex,illumination.getLayer(globalParameters.buildOcclusion ? LAYER_OCCLUSION : LAYER_LIGHTMAP),directionalBuffers,illumination.getLayer(LAYER_BENT_NORMALS),&updateParameters,NULL);
				// postprocess
				objectParameters.layersPostprocess(&scene.objects[objectIndex]->illumination);
				// save
				saved += objectParameters.layersSave(&scene.objects[objectIndex]->illumination);
				// free
				if (!globalParameters.runViewer)
					objectParameters.layersDelete(&scene.objects[objectIndex]->illumination);
			}
		}

#ifdef _WIN32
		// restore priority
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
#endif

		// restore light intensities, just in case we are going to run viewer
		for (unsigned i=0;i<solver->getLights().size();i++)
		{
			solver->getLights()[i]->color = lightColors[i];
		}

		rr::RRReporter::report(rr::INF2,"Saved %d files.\n",saved);
		// saving 0 files is strange, force user to read log and quit
		if (!saved)
			error(solver->aborting,NULL);
	}

	//
	// run interactive scene viewer
	//
#ifdef SCENE_VIEWER
	if (globalParameters.runViewer || !globalParameters.buildQuality)
	{
		rr_gl::SceneViewerState svs;
		svs.tonemappingAutomatic = false;
		svs.renderHelpers = true;
		if (globalParameters.buildQuality)
		{
			// switch from default realtime GI to static GI
			svs.renderLightDirect = rr_gl::LD_STATIC_LIGHTMAPS;
			svs.renderLightIndirect = rr_gl::LI_STATIC_LIGHTMAPS;
		}
		svs.staticLayerNumber = globalParameters.buildOcclusion ? LAYER_OCCLUSION : LAYER_LIGHTMAP; // switch from default layer to our layer 0
		rr::RRReporter::report(rr::INF1,"Stopping this log, additional mesages go to log in Scene viewer.\n");
		RR_SAFE_DELETE(reporter);
#ifdef NDEBUG
		// release returns quickly without freeing resources
		rr_gl::sceneViewer(solver,globalParameters.sceneFilename,NULL,"../../data/shaders/",&svs,false);
#else
		// debug frees everything and reports memory leaks
		rr_gl::sceneViewer(solver,globalParameters.sceneFilename,NULL,"../../data/shaders/",&svs,true);
		rr_gl::deleteAllTextures();
#endif
	}
#endif

	//
	// free memory
	//
#ifndef NDEBUG
	delete solver->getEnvironment();
	delete solver->getScaler();
	RR_SAFE_DELETE(solver); // sets solver to NULL (it's important because reporter still references solver)
	delete reporter;
#endif

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
// WinMain() calls main(), for compatibility with Windows

#ifdef _WIN32
int parseCommandline(const wchar_t* commandline)
{
	int argc = 0;
	LPWSTR* argvw = CommandLineToArgvW(commandline, &argc);
	if (argvw && argc)
	{
		// build argv from commandline
		char** argv = new char*[argc+1];
		for (int i=0;i<argc;i++)
		{
			argv[i] = (char*)malloc(wcslen(argvw[i])+1);
			sprintf(argv[i], "%ws", argvw[i]);
		}
		argv[argc] = NULL;
		if (argvw && argc==2 && argvw[1][0]=='@')
		{
			// read argv from file
			std::wstring wstr = std::wstring(L"\"")+argvw[0]+L"\" ";
			FILE* f = fopen(argv[1]+1,"rt");
			if (f)
			{
				unsigned char c;
				while (fread(&c,1,1,f)==1)
					wstr += c;
				fclose(f);
				RR_LIMITED_TIMES(1,return parseCommandline(wstr.c_str()));
			}
		}
		return main(argc,argv);
	}
	else
	{
		// someone calls us with invalid arguments, but don't panic, build argv from module filename
		char szFileName[MAX_PATH];
		GetModuleFileNameA(NULL,szFileName,MAX_PATH);
		char* argv[2] = {szFileName,NULL};
		return main(1,argv);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
	return parseCommandline(GetCommandLineW());
}
#endif
