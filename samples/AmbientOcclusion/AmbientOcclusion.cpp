// --------------------------------------------------------------------------
// Ambient Occlusion sample
//
// This sample computes Ambient Occlusion maps 
// including infinite light bounces and color bleeding.
//
// It loads Collada 1.4 .DAE scene, calculates Ambient Occlusion maps,
// saves them to disk and runs interactive scene/lightmap viewer.
//
// The only lightsource is white skybox.
// You can quickly change sky or add other light sources
// (lights, emissive materials).
//
// Remarks:
// - map quality depends on unwrap quality,
//   make sure you have good unwrap in your scenes
//   (save it as second TEXCOORD in Collada document, see RRObjectCollada.cpp)
// - light leaks below wall due to bug in scene geometry,
//   scene behind wall is bright, wall is infinitely thin and there's no seam
//   in mapping between bright and dark part of floor.
//   seam in floor mapping or thick wall would fix it
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007-2008
// --------------------------------------------------------------------------

#define SELECTED_OBJECT_NUMBER 0 // selected object gets per-pixel AO, others get per-vertex AO
//#define TB

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>    // SetPriorityClass
#endif

#include "Lightsprint/RRMath.h"    // TODO: Platform.h
#ifdef TB
#include "../../samples/ImportTB/RRObjectTB.h"
#endif

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/IO/ImportScene.h"


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx, 9xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// main

void calculate(rr::RRDynamicSolver* solver, unsigned layerNumber)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
	{
		unsigned numVertices = solver->getObject(i)->getCollider()->getMesh()->getNumVertices();
		if(numVertices)
		{
			solver->getIllumination(i)->getLayer(layerNumber) =
				(i==SELECTED_OBJECT_NUMBER)
				?
				// allocate lightmap for selected object
				rr::RRBuffer::create(rr::BT_2D_TEXTURE,256,256,1,rr::BF_RGB,true,NULL)
				:
				// allocate vertex buffers for other objects
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVertices,1,1,rr::BF_RGBF,false,NULL);
		}
	}

	// calculate ambient occlusion
	rr::RRDynamicSolver::UpdateParameters params(2000);
	rr::RRDynamicSolver::FilteringParameters filtering;
	filtering.wrap = false;
#ifdef _WIN32
	rr_gl::updateLightmapsWithDialog(solver,layerNumber,-1,-1,&params,&params,&filtering,true,"../../data/shaders/",NULL);
#else
	solver->updateLightmaps(layerNumber,-1,-1,&params,&params,&filtering); 
#endif
}

int main(int argc, char **argv)
{
	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());

	rr_io::setImageLoader();

	// init scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
#ifdef TB
	// these are our peice chunk AO textures names to match on
	std::vector< std::string > mapNames;
	//mapNames.push_back( "1_0" );
	mapNames.push_back( "1_1" );
	//mapNames.push_back( "1_2" );
	//mapNames.push_back( "1_3" );
	//mapNames.push_back( "1_4" );
	//mapNames.push_back( "1_5" );
	//mapNames.push_back( "1_6" );
	//mapNames.push_back( "1_7" );
	//mapNames.push_back( "1_8" );
	//mapNames.push_back( "1_9" ); //missing
	//mapNames.push_back( "1_10" ); //missing
	//mapNames.push_back( "1_11" );
	//mapNames.push_back( "1_12" );
	//mapNames.push_back( "1_13" );
	//mapNames.push_back( "1_14" );
	solver->setStaticObjects( *adaptObjectsFromTB( mapNames ), NULL );
#else

	// load scene
	rr_io::ImportScene scene("../../data/scenes/koupelna/koupelna4-windows.dae");
	solver->setStaticObjects(*scene.getObjects(), NULL);

#endif
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);
	
	// set white environment
	solver->setEnvironment( rr::RRBuffer::createSky() );

	{
		rr::RRReportInterval report(rr::INF1,"Starting AO lightmap build, takes approx 2 minutes ...\n");

		// decrease priority, so that this task runs on background using only free CPU cycles
#ifdef _WIN32
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#endif

		// calculate and save it
		calculate(solver,0);
		solver->getStaticObjects().saveIllumination("../../data/export/",0);

		// or load it
		//solver->getObjects()->loadIllumination("../../data/export/",0);

		// restore priority
#ifdef _WIN32
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
#endif
	}

	rr_gl::SceneViewerState svs;
	svs.renderRealtime = 0; // switch from default realtime GI to static GI
	svs.staticLayerNumber = 0; // switch from default layer to our layer 0
	rr_gl::sceneViewer(solver,true,"../../data/shaders/",&svs);

	return 0;
}
