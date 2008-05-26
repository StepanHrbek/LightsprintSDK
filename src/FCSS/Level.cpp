#define QUAKE_DIR_STRUCTURE        false

#include "Level.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "GL/glew.h"

extern rr_gl::RRDynamicSolverGL* createSolver();
extern void error(const char* message, bool gfxRelated);

/////////////////////////////////////////////////////////////////////////////
//
// Level body

Level::Level(LevelSetup* levelSetup, rr::RRBuffer* skyMap, bool supportEditor) : pilot(levelSetup)
{
	rr::RRReportInterval report(rr::INF1,"Loading %s...\n",pilot.setup->filename);

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
	solver->setScaler(rr::RRScaler::createFastRgbScaler());
	solver->setEnvironment(skyMap);

	/*
	if(strstr(filename, "candella"))
	{
		rr_gl::Camera tmpeye = {{885.204,13.032,537.904},2.050,0,10.000,1.3,100.0,0.3,1000.0};
		rr_gl::Camera tmplight = {{876.157,13.782,521.345},7.430,0,2.350,1.0,70.0,1.0,100.0};
		eye = tmpeye;
		light = tmplight;
	}*/

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
			char* maps = _strdup(pilot.setup->filename);
			char* mapsEnd;
			if(QUAKE_DIR_STRUCTURE)
			{
				mapsEnd = MAX(strrchr(maps,'\\'),strrchr(maps,'/')); if(mapsEnd) mapsEnd[0] = 0;
			}
			mapsEnd = MAX(strrchr(maps,'\\'),strrchr(maps,'/')); if(mapsEnd) mapsEnd[1] = 0;
			//char missing[1000];
			//strncpy(missing,maps,999);
			//missing[970] = 0;
			//strcat(missing,"/missing.jpg");
			//objects = adaptObjectsFromTMapQ3(&bsp,maps,rr_gl::Texture::load(missing,NULL,false,false,GL_LINEAR,GL_LINEAR_MIPMAP_LINEAR,GL_REPEAT,GL_REPEAT));
			objects = adaptObjectsFromTMapQ3(&bsp,maps,!QUAKE_DIR_STRUCTURE,NULL);
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
			objects = adaptObjectsFrom3DS(&m3ds);
			break;
		}
#endif
#ifdef SUPPORT_COLLADA
		case TYPE_DAE:
		{
			// load collada document
			FCollada::Initialize();
			collada = FCollada::NewTopDocument();
			FUErrorSimpleHandler errorHandler;
			FCollada::LoadDocumentFromFile(collada,pilot.setup->filename);
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

	rr::RRDynamicSolver::SmoothingParameters sp;
	//zapnuti techto 2 radek zrychli sponzu o 15% 
	//sp.minFeatureSize = 0.15f;
	//sp.vertexWeldDistance = 0.01f; // pri 1cm spekal podlahy v flat1, pri 1mm spekal podlahu a strop v flat3
	//sp.vertexWeldDistance = -1; // vypnuty weld by teoreticky nemel skodit, ale prakticky zpomaluje updatevbuf i render(useOriginal), nevim proc
	//sp.vertexWeldDistance = -1; // vypnuty weld mozna zlepsi nahravani map z WoP
	sp.minFeatureSize = pilot.setup->minFeatureSize;
#ifdef THREE_ONE
	sp.intersectTechnique = rr::RRCollider::IT_BSP_FASTEST;
#endif
	solver->setStaticObjects(*objects,&sp);
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
		solver->getIllumination(i)->getLayer(0) =
			rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);

	// init light
	//rr::RRLights lights;
	//lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(-1.802,0.715,0.850),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),40*3.14159f/180,0.1f));
	//solver->setLights(lights);

	// load Fireball
	char* fbname = _strdup(pilot.setup->filename);
	strcpy(fbname+strlen(fbname)-3,"fib");
	solver->loadFireball(fbname) || solver->buildFireball(1000,fbname);
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
			collada->Release();
			FCollada::Release();
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

unsigned Level::saveIllumination(const char* path)
{
	unsigned result = 0;
	if(pilot.setup && solver)
	{
		for(LevelSetup::Frames::iterator i=pilot.setup->frames.begin();i!=pilot.setup->frames.end();i++)
		{
			result += solver->getStaticObjects().saveIllumination(path,(*i)->layerNumber);
		}
	}
	return result;
}

unsigned Level::loadIllumination(const char* path)
{
	unsigned result = 0;
	if(pilot.setup && solver)
	{
		for(LevelSetup::Frames::iterator i=pilot.setup->frames.begin();i!=pilot.setup->frames.end();i++)
		{
			result += solver->getStaticObjects().loadIllumination(path,(*i)->layerNumber);
		}
	}
	return result;
}
