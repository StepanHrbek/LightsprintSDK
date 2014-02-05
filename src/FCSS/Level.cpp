#include "Level.h"
#include "GL/glew.h"

const unsigned REBUILD_FIB = 0; // 1 = rebuild precomputed .fib (fireball)          0 = build at low quality only if it's missing
const unsigned REBUILD_JPG = 0; // 1 = rebuild precomputed .jpg (light detail map)  0 = build at low quality only if it's missing
// jeste pomaha v RRMaterial.cpp zmenit "minimalQualityForPointMaterials =" na 1, udela vsude point matrose

extern void error(const char* message, bool gfxRelated);

/////////////////////////////////////////////////////////////////////////////
//
// Level body

Level::Level(LevelSetup* levelSetup, rr::RRBuffer* skyMap, bool supportEditor)
{
	// loading scenename will be reported by RRScene, don't make it confusing by 2 reports
	//rr::RRReportInterval report(rr::INF1,"Loading %s...\n",setup->filename);

	setup = levelSetup;
	animationEditor = supportEditor ? new AnimationEditor(levelSetup) : NULL;
	solver = NULL;

	// init radiosity solver
	extern rr_gl::RRSolverGL::DDIQuality lightStability;
	solver = new rr_gl::RRSolverGL("shaders/","maps/",lightStability);
	solver->setDirectIlluminationBoost(2);
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createFastRgbScaler());
	solver->setEnvironment(skyMap);

	scene = new rr::RRScene(setup->filename);

	if (!scene->objects.size())
	{
		rr::RRReporter::report(rr::ERRO,"Scene %s not loaded.\n",setup->filename);
		error("",false);
	}

	scene->normalizeUnits(setup->scale);

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
	if (!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init light
	//rr::RRLights lights;
	//lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(-1.802,0.715,0.850),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),RR_DEG2RAD(40),0.1f));
	//solver->setLights(lights);

	// load light detail map
	{
		char* ldmName = _strdup(setup->filename);
		strcpy(ldmName+strlen(ldmName)-3,"jpg");
		rr::RRBuffer* ldm = REBUILD_JPG ? NULL : rr::RRBuffer::load(ldmName);
		if (!ldm)
		{
			// build light detail map
			solver->getStaticObjects()[0]->illumination.getLayer(getLDMLayer()) = ldm = rr::RRBuffer::create(rr::BT_2D_TEXTURE,2048,2048,1,rr::BF_RGB,true,NULL);
			rr::RRSolver::UpdateParameters paramsDirect(REBUILD_JPG ? 2000 : 20);
			paramsDirect.applyLights = 0;
			paramsDirect.aoIntensity = 2;
			paramsDirect.aoSize = 1;
			rr::RRSolver::UpdateParameters paramsIndirect(REBUILD_JPG ? 2000 : 20);
			paramsIndirect.applyLights = 0;
			paramsIndirect.locality = -1;
			paramsIndirect.qualityFactorRadiosity = 0;
			rr::RRBuffer* oldEnv = solver->getEnvironment();
			rr::RRBuffer* newEnv = rr::RRBuffer::createSky(rr::RRVec4(0.6f,0.7f,0.75f,1),rr::RRVec4(0.6f,0.7f,0.75f,1)); // kompenzuju tmave hnedy barvy padattic aby vysledna LDM obsahovala barvy kolem 0.5
			solver->setEnvironment(newEnv);
			rr::RRSolver::FilteringParameters filtering;
			filtering.backgroundColor = rr::RRVec4(0.5f);
			filtering.wrap = false;
			solver->updateLightmaps(getLDMLayer(),-1,-1,&paramsDirect,&paramsIndirect,&filtering); 
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
		solver->getStaticObjects()[0]->illumination.getLayer(getLDMLayer()) = ldm;
	}

	// load Fireball
	char* fbname = _strdup(setup->filename);
	strcpy(fbname+strlen(fbname)-3,"fib");
	if (REBUILD_FIB)
		solver->buildFireball(5000,fbname);
	else
		solver->loadFireball(fbname,false) || solver->buildFireball(1000,fbname);
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

	//printf("After optimizations: vertices=%d, triangles=%d.\n",solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumVertices(),solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles());
}

Level::~Level()
{
	if (animationEditor)
		setup->save();
	delete animationEditor;
	delete solver->getScaler();
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
