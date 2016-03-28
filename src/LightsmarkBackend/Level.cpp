#include "Level.h"

const unsigned REBUILD_FIB = 0; // 1 = rebuild precomputed .fib (fireball)          0 = build at low quality only if it's missing
const unsigned REBUILD_JPG = 0; // 1 = rebuild precomputed .jpg (light detail map)  0 = build at low quality only if it's missing
// jeste pomaha v RRMaterial.cpp zmenit "minimalQualityForPointMaterials =" na 1, udela vsude point matrose
// bez toho by svetlost LDM zavisela na quality (mensi q = casteji saha na color, ktera je umele zesvetlena; vetsi q = casteji saha na tex, ktera je tmavsi)

#define MATERIAL_MULTIPLIER 1 // replaces DIFFUSE_MULTIPLIER in .bsp loader

extern void error(const char* message, bool gfxRelated);

/////////////////////////////////////////////////////////////////////////////
//
// Level body

Level::Level(LevelSetup* levelSetup, rr::RRBuffer* skyMap, bool supportEditor)
{
	// loading scenename will be reported by RRScene, don't make it confusing by 2 reports
	//rr::RRReportInterval report(rr::INF1,"Loading %s...\n",setup->filename);

	setup = levelSetup;
	animationEditor = supportEditor ? new AnimationEditor(levelSetup) : nullptr;
	solver = nullptr;

	// init radiosity solver
	extern rr_gl::RRSolverGL::DDIQuality lightStability;
	solver = new rr_gl::RRSolverGL("shaders/","maps/",lightStability);
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setColorSpace(rr::RRColorSpace::create_sRGB());
	solver->setEnvironment(skyMap);

	scene = new rr::RRScene(setup->filename);

	if (!scene->objects.size())
	{
		rr::RRReporter::report(rr::ERRO,"Scene %s not loaded.\n",setup->filename);
		error("",false);
	}

	scene->normalizeUnits(setup->scale);

	// modify reflectance to be more like in Lightsmark 2007/8
	// this is optional step, reduces realism
	rr::RRMaterials materials;
	scene->objects.getAllMaterials(materials);
	for (unsigned i=0;i<materials.size();i++)
	{
		// recalculate color without sRGB correction
		materials[i]->updateColorsFromTextures(nullptr,rr::RRMaterial::UTA_DELETE,true);
		// make materials reflect more indirect light, to make effect more visible
		materials[i]->diffuseReflectance.color *= MATERIAL_MULTIPLIER;
	}

	// smoothuje scenu.
	// pozor, odstrani pritom triangly v zavislosti na vysledcich floatovych operaci, ruzna cpu ruzne triangly.
	// jine cpu pak nemuze pouzit mnou predpocitany .fireball
	// a) nesmoothovat
	// b) smoothovat a pocitat fireball u uzivatele 
	// c) smoothovat, dodat svuj fireball, pripustit ze nekteri uzivatele si spocitaj jinej (musel by mit stejnou kvalitu)
	rr::RRSolver::SmoothingParameters sp;
	sp.vertexWeldDistance = 0.01f; // akorat dost aby sesmoothoval sane ve wop_padattic (nicmene pri 1cm speka podlahy v flat1, pri 1mm speka podlahu a strop v flat3)
	sp.maxSmoothAngle = 0.5; // akorat dost aby sesmoothoval sane ve wop_padattic
	solver->setStaticObjects(scene->objects,&sp);
	if (!solver->getMultiObject())
		error("No objects in scene.",false);

	// init light
	//rr::RRLights lights;
	//lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(-1.802,0.715,0.850),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),RR_DEG2RAD(40),0.1f));
	//solver->setLights(lights);

	// load light detail map
	{
		char* ldmName = _strdup(setup->filename);
		strcpy(ldmName+strlen(ldmName)-3,"jpg");
		rr::RRBuffer* ldm = REBUILD_JPG ? nullptr : rr::RRBuffer::load(ldmName);
		if (!ldm)
		{
			// build light detail map
			// since Lightsmark 2008, this LDM builder stopped darkening floor under boxes
			// old behavior can be restored by #define BACKSIDE_ILLEGAL and #define TEST_BIT(material) 1 at proper places
			solver->getStaticObjects()[0]->illumination.getLayer(LAYER_LDM) = ldm = rr::RRBuffer::create(rr::BT_2D_TEXTURE,2048,2048,1,rr::BF_RGB,true,nullptr);
			rr::RRSolver::UpdateParameters params(REBUILD_JPG ? 2000 : 20);
			params.direct.lightMultiplier = 0;
			params.indirect.lightMultiplier = 0;
			params.aoIntensity = 2;
			params.aoSize = 1;
			params.locality = -1; // used to be in indirect only
			params.qualityFactorRadiosity = 0; // used to be in indirect only
			rr::RRBuffer* oldEnv = solver->getEnvironment();
			rr::RRBuffer* newEnv = rr::RRBuffer::createSky(rr::RRVec4(0.6f,0.7f,0.75f,1),rr::RRVec4(0.6f,0.7f,0.75f,1)); // kompenzuju tmave hnedy barvy padattic aby vysledna LDM obsahovala barvy kolem 0.5
			solver->setEnvironment(newEnv);
			rr::RRSolver::FilteringParameters filtering;
			filtering.backgroundColor = rr::RRVec4(0.5f);
			filtering.wrap = false;
			//filtering.spreadForegroundColor = 0; // 0 is better for debugging, default 1000 is better for jpg compression as it reduces sharp edges
			filtering.smoothingAmount = 0; // LDM is made once, we can afford so high quality that filtering is not needed
			solver->updateLightmaps(LAYER_LDM,-1,-1,&params,&filtering); 
			solver->setEnvironment(oldEnv);
			delete newEnv;
			rr::RRVec4 mini(1),maxi(0);
			ldm->getMinMax(&mini,&maxi);
			if (mini==rr::RRVec4(0,0,0,1) && maxi==rr::RRVec4(0,0,0,1))
			{
				// LDM completely black, something went wrong (e.g. missing unwrap), don't save it, don't use it (it would black indirect)
				RR_SAFE_DELETE(ldm);
			}
			else
			{
				// save light detail map
				ldm->save(ldmName);
				// save copy to .png
				if (REBUILD_JPG)
				{
					strcpy(ldmName+strlen(ldmName)-3,"png");
					ldm->save(ldmName);
				}
			}
		}
		free(ldmName);
		solver->getStaticObjects()[0]->illumination.getLayer(LAYER_LDM) = ldm;
	}

	// load Fireball
	char* fbname = _strdup(setup->filename);
#ifdef NDEBUG
	strcpy(fbname+strlen(fbname)-3,"fib");
#else
	strcpy(fbname+strlen(fbname)-3,"fid");
#endif
	if (REBUILD_FIB)
		solver->buildFireball(5000,fbname);
	else
		solver->loadFireball(fbname,false) || solver->buildFireball(1000,fbname);
	free(fbname);

	/*/ autodetect positions in center of scene
	rr::RRMesh* mesh = solver->getMultiObject()->getCollider()->getMesh();
	rr::RRVec3 center;
	rr::RRVec3 mini;
	rr::RRVec3 maxi;
	mesh->getAABB(&mini,&maxi,&center);
	rr::RRVec3 size = maxi-mini;
	rr::RRVec3 bestPos = center;
	rr::RRReal bestValue = 0;
	for (unsigned i=0;i<1000;i++)
	{
		rr::RRVec3 pos = center + i/1000.f*(RRVec3(size[0]*(rand()/(RRReal)RAND_MAX-0.5f),size[1]*(rand()/(RRReal)RAND_MAX-0.5f),size[2]*(rand()/(RRReal)RAND_MAX-0.5f));
		rr::RRReal val = ;
		if (val>bestValue)
		{
			bestValue = val;
			bestPos = pos;
		}
	}
	eye.pos = bestPos;
	*/

	//printf("After optimizations: vertices=%d, triangles=%d.\n",solver->getMultiObject()->getCollider()->getMesh()->getNumVertices(),solver->getMultiObject()->getCollider()->getMesh()->getNumTriangles());
}

Level::~Level()
{
	if (animationEditor)
		setup->save();
	delete animationEditor;
	delete solver->getColorSpace();
	delete solver;
	delete scene;
}

unsigned Level::saveIllumination(const char* path)
{
	unsigned result = 0;
	if (setup && solver)
	{
		for (LevelSetup::Frames::iterator i=setup->frames.begin();i!=setup->frames.end();i++)
		{
			result += solver->getStaticObjects().saveLayer((*i)->layerNumber,path,"png");
		}
	}
	return result;
}

unsigned Level::loadIllumination(const char* path)
{
	unsigned result = 0;
	if (setup && solver)
	{
		for (LevelSetup::Frames::iterator i=setup->frames.begin();i!=setup->frames.end();i++)
		{
			result += solver->getStaticObjects().loadLayer((*i)->layerNumber,path,"png");
		}
	}
	return result;
}
