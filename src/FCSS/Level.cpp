#define SUBDIVISION                0

#include "Level.h"
#include "Lightsprint/GL/RendererOfRRObject.h"

extern rr::RRDynamicSolver* createSolver();
extern void error(const char* message, bool gfxRelated);

/////////////////////////////////////////////////////////////////////////////
//
// Level body

Level::Level(LevelSetup* levelSetup, rr::RRIlluminationEnvironmentMap* skyMap, bool supportEditor) : pilot(levelSetup)
{
	animationEditor = supportEditor ? new AnimationEditor(levelSetup) : NULL;
	solver = NULL;
#ifdef BUGS
	bugs = NULL;
#endif
	rendererOfScene = NULL;
	objects = NULL;
#ifdef SUPPORT_COLLADA
	collada = NULL;
#endif
	// init radiosity solver
	solver = createSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	solver->setEnvironment(skyMap);

	/*
	if(strstr(filename, "candella"))
	{
		rr_gl::Camera tmpeye = {{885.204,13.032,537.904},2.050,0,10.000,1.3,100.0,0.3,1000.0};
		rr_gl::Camera tmplight = {{876.157,13.782,521.345},7.430,0,2.350,1.0,70.0,1.0,100.0};
		eye = tmpeye;
		light = tmplight;
	}*/

	rr::RRReporter::report(rr::INF1,"Loading %s...",pilot.setup->filename);

	type = TYPE_NONE;
	const char* typeExt[] = {".3ds",".bsp",".dae",".mgf"};
	for(unsigned i=TYPE_3DS;i<TYPE_NONE;i++)
	{
		if(strlen(pilot.setup->filename)>=4 && _stricmp(pilot.setup->filename+strlen(pilot.setup->filename)-4,typeExt[i])==0)
		{
			type = (Level::Type)i;
			break;
		}
	}

	switch(type)
	{
#ifdef SUPPORT_BSP
		case TYPE_BSP:
		{
			// load quake 3 map
			if(!readMap(pilot.setup->filename,bsp))
				error("Failed to load .bsp scene.",false);
			rr::RRReporter::report(rr::CONT,"\n");
			char* maps = _strdup(pilot.setup->filename);
			char* mapsEnd;
			mapsEnd = MAX(strrchr(maps,'\\'),strrchr(maps,'/')); if(mapsEnd) mapsEnd[0] = 0;
			mapsEnd = MAX(strrchr(maps,'\\'),strrchr(maps,'/')); if(mapsEnd) mapsEnd[1] = 0;
			//rr_gl::Texture::load("maps/missing.jpg",NULL);
			objects = adaptObjectsFromTMapQ3(&bsp,maps,NULL);
			free(maps);
			break;
		}
#endif
#ifdef SUPPORT_3DS
		case TYPE_3DS:
		{
			// load .3ds scene
			if(!m3ds.Load(pilot.setup->filename,pilot.setup->scale))
				error("",false);
			rr::RRReporter::report(rr::CONT,"\n");
			objects = adaptObjectsFrom3DS(&m3ds);
			break;
		}
#endif
#ifdef SUPPORT_COLLADA
		case TYPE_DAE:
		{
			// load collada document
			collada = FCollada::NewTopDocument();
			FUErrorSimpleHandler errorHandler;
			collada->LoadFromFile(pilot.setup->filename);
			rr::RRReporter::report(rr::CONT,"\n");
			if(!errorHandler.IsSuccessful())
			{
				puts(errorHandler.GetErrorString());
				SAFE_DELETE(collada);
			}
			else
			{
				objects = adaptObjectsFromFCollada(collada);
			}
			break;
		}
#endif
#ifdef SUPPORT_MGF
		case TYPE_MGF:
			objects = adaptObjectsFromMGF(pilot.setup->filename);
			break;
#endif
		default:
			puts("Unsupported scene format.");
	}
	if(!objects || !objects->size())
	{
		rr::RRReporter::report(rr::ERRO,"Scene %s not loaded.\n",pilot.setup->filename);
		error("",false);
	}

	rr::RRStaticSolver::SmoothingParameters sp;
	sp.subdivisionSpeed = SUBDIVISION;
	//zapnuti techto 2 radek zrychli sponzu o 15% 
	//sp.minFeatureSize = 0.15f;
	//sp.vertexWeldDistance = 0.01f; // pri 1cm spekal podlahy v flat1, pri 1mm spekal podlahu a strop v flat3
#ifdef THREE_ONE
	sp.intersectTechnique = rr::RRCollider::IT_BSP_FASTEST;
#endif
	solver->setObjects(*objects,&sp);

	//	printf(solver->getObject(0)->getCollider()->getMesh()->save("c:\\a")?"saved":"not saved");
	//	printf(solver->getObject(0)->getCollider()->getMesh()->load("c:\\a")?" / loaded":" / not loaded");

	//solver->calculate(); // creates radiosity solver with multiobject. without renderer, no primary light is detected
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// load Fireball
	char* fbname = _strdup(pilot.setup->filename);
	strcpy(fbname+strlen(fbname)-3,"fib");
	if(!solver->setFireball(fbname))
	{
		solver->buildFireball(1000,fbname);
		solver->setFireball(fbname);
	}
	free(fbname);

	/*/ autodetect positions in center of scene
	rr::RRMesh* mesh = solver->getMultiObjectCustom()->getCollider()->getMesh();
	rr::RRVec3 center;
	rr::RRVec3 mini;
	rr::RRVec3 maxi;
	mesh->getAABB(&mini,&maxi,&center);
	rr::RRVec3 size = maxi-mini;
	rr::RRVec3 bestPos = center;
	rr::RRReal bestValue = 0;
	for(unsigned i=0;i<1000;i++)
	{
		rr::RRVec3 pos = center + i/1000.f*(RRVec3(size[0]*(rand()/(RRReal)RAND_MAX-0.5f),size[1]*(rand()/(RRReal)RAND_MAX-0.5f),size[2]*(rand()/(RRReal)RAND_MAX-0.5f));
		rr::RRReal val = ;
		if(val>bestValue)
		{
			bestValue = val;
			bestPos = pos;
		}
	}
	eye.pos = bestPos;
	*/

	// init renderer
	rendererOfScene = new rr_gl::RendererOfScene(solver,"shaders/");

	//printf("After optimizations: vertices=%d, triangles=%d.\n",solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumVertices(),solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles());

#ifdef BUGS
	// init bugs
	bugs = Bugs::create(solver->getStaticSolver(),solver->getMultiObjectCustom(),100);
#endif
}

Level::~Level()
{
	if(animationEditor)
		pilot.setup->save();
	delete animationEditor;

	switch(type)
	{
#ifdef SUPPORT_BSP
		case TYPE_BSP:
			freeMap(bsp);
			break;
#endif
#ifdef SUPPORT_3DS
		case TYPE_3DS:
			break;
#endif
#ifdef SUPPORT_COLLADA
		case TYPE_DAE:
			delete collada;
			break;
#endif
#ifdef SUPPORT_MGF
		case TYPE_MGF:
			break;
#endif
		default:
			puts("Unsupported scene format.");
	}
#ifdef BUGS
	delete bugs;
#endif
	delete rendererOfScene;
	delete solver->getScaler();
	delete solver;
	delete objects;
}

unsigned Level::saveIllumination(const char* path, bool vertexColors, bool lightmaps)
{
	unsigned result = 0;
	if(pilot.setup && solver)
	{
		rr_gl::RRDynamicSolverGL* solverGL = (rr_gl::RRDynamicSolverGL*)solver; //!!!
		for(LevelSetup::Frames::iterator i=pilot.setup->frames.begin();i!=pilot.setup->frames.end();i++)
		{
			result += solverGL->saveIllumination(path,(*i)->layerNumber,vertexColors,lightmaps);
		}
	}
	return result;
}

unsigned Level::loadIllumination(const char* path, bool vertexColors, bool lightmaps)
{
	unsigned result = 0;
	if(pilot.setup && solver)
	{
		rr_gl::RRDynamicSolverGL* solverGL = (rr_gl::RRDynamicSolverGL*)solver; //!!!
		for(LevelSetup::Frames::iterator i=pilot.setup->frames.begin();i!=pilot.setup->frames.end();i++)
		{
			result += solverGL->loadIllumination(path,(*i)->layerNumber,vertexColors,lightmaps);
		}
	}
	return result;
}
