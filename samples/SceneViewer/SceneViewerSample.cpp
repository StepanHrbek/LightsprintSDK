// --------------------------------------------------------------------------
// SceneViewer sample
//
// Quick simple scene viewer with
// - realtime GI: all lights freely movable
// - offline GI: build/save/load lightmaps
//
// This is implemented in two steps
// 1. load scene (Collada .dae, .3ds, Quake3 .bsp, .mgf)
// 2. call sceneViewer() function
//
// Use commandline argument or drag&drop to open custom scene.
//
// Controls:
// right mouse button ... menu
// arrows or wsadqz   ... movement
// wheel              ... zoom
// + -                ... brightness adjustment
//
// If it doesn't render your scene properly, please send us sample so we can fix it.
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007-2008
// --------------------------------------------------------------------------

#include "../Import/ImportScene.h"
#include <cstdlib>
#include <cstdio>
#include <crtdbg.h>
#include <direct.h>
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/Texture.h"

void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if(gfxRelated)
		rr::RRReporter::report(rr::ERRO,"\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx, 9xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

int main(int argc, char **argv)
{
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 65;

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
	//rr::RRReporter::setFilter(true,3,true);
	//rr_gl::Program::logMessages(1);
	
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = _strdup(argv[0]);
	for(unsigned i=strlen(exedir);--i;) if(exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	_chdir(exedir);
	free(exedir);

	// load scene
	ImportScene scene((argc>1)?argv[1]:"../../data/scenes/koupelna/koupelna4.dae");

	// init solver
	if(rr::RRLicense::loadLicense("../../data/licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	solver->setScaler(rr::RRScaler::createRgbScaler()); // switch inputs and outputs from HDR physical scale to RGB screenspace
	if(scene.getObjects()) solver->setStaticObjects(*scene.getObjects(),NULL);
	if(scene.getLights()) solver->setLights(*scene.getLights());
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(rr::RRBuffer::load("../../data/maps/skybox/skybox_%s.jpg",cubeSideNames,true,true));

	// run interactive scene viewer
	rr_gl::sceneViewer(solver,true,"../../data/shaders/",-1,false);

	rr_gl::deleteAllTextures();
	delete solver->getEnvironment();
	delete solver->getScaler();
	delete solver;
	delete rr::RRReporter::getReporter();
	rr::RRReporter::setReporter(NULL);

	return 0;
}
