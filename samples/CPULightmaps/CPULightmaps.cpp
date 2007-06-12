// --------------------------------------------------------------------------
// CPU Lightmaps sample
//
// This sample computes lightmaps with global illumination on CPU.
// GPU is not required.
//
// It loads Collada 1.4 .DAE scene, calculates lightmaps/vertxcolors
// and saves them to disk.
//
// Lightsources considered are
// - point, spot and directional lights in scene
//   (see supported light features in RRObjectCollada.cpp)
// - skybox loaded from textures, separately from scene
//
// Remarks:
// - map quality depends on unwrap quality,
//   make sure you have good unwrap in your scenes
//   (save it as second TEXCOORD in Collada document, see RRObjectCollada.cpp)
// - disable #define OPENGL in RRObjectCollada.cpp to load Collada 
//   scene textures to system memory, rather than to OpenGL
// - idle time of all CPUs/cores is used, so other applications don't suffer
//   from lightmap precalculator running on the background
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// --------------------------------------------------------------------------

#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "../ImportCollada/RRObjectCollada.h"

#include <cassert>
#include <cstdlib>
#include <ctime>


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// only map creation is implemented here

class Solver : public rr::RRDynamicSolver
{
public:
	Solver() {}
protected:
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		// Decide how big ambient occlusion map you want for object.
		// In this sample, we pick res proportional to number of triangles in object.
		// Optimal res depends on quality of unwrap provided by object->getTriangleMapping.
		unsigned res = 16;
		unsigned sizeFactor = 5; // higher factor = higher map resolution
		while(res<2048 && res<sizeFactor*sqrtf((float)object->getCollider()->getMesh()->getNumTriangles())) res*=2;
		return rr::RRIlluminationPixelBuffer::create(res,res);
	}
	virtual void detectMaterials() {}
	virtual bool detectDirectIllumination() {return false;}
	virtual void setupShader(unsigned objectNumber) {}
};


void calculatePerVertexAndSelectedPerPixel(rr::RRDynamicSolver* solver, unsigned layerNumber)
{
	// calculate per vertex - all objects
	// it is faster and quality is good for some objects
	rr::RRDynamicSolver::UpdateParameters paramsDirect;
	paramsDirect.measure.scaled = false; // get vertex colors in HDR
	paramsDirect.quality = 5000;
	paramsDirect.applyCurrentSolution = false;
	paramsDirect.applyEnvironment = true;
	paramsDirect.applyLights = true;
	rr::RRDynamicSolver::UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = true;
	paramsIndirect.applyLights = true;
	solver->updateVertexBuffers(0,true,&paramsDirect,&paramsIndirect); 

	// calculate per pixel - selected objects
	// it is slower, but some objects need it
	rr::RRDynamicSolver::UpdateParameters paramsDirectPixel;
	paramsDirectPixel.measure.scaled = true; // get maps in sRGB
	paramsDirectPixel.quality = 2000;
	paramsDirectPixel.applyEnvironment = true;
	paramsDirectPixel.applyLights = true;
	unsigned objectNumbers[] = {3};
	for(unsigned i=0;i<sizeof(objectNumbers)/sizeof(objectNumbers[0]);i++)
	{
		unsigned objectNumber = objectNumbers[i];
		if(solver->getObject(objectNumber))
		{
			solver->getIllumination(objectNumber)->getLayer(layerNumber)->pixelBuffer = rr::RRIlluminationPixelBuffer::create(256,256);
			solver->updateLightmap(objectNumber,solver->getIllumination(objectNumber)->getLayer(layerNumber)->pixelBuffer,&paramsDirectPixel);
		}
	}
}

void calculatePerPixel(rr::RRDynamicSolver* solver, unsigned layerNumber)
{
	// calculate per pixel - all objects
	rr::RRDynamicSolver::UpdateParameters paramsDirect;
	paramsDirect.measure.scaled = false;
	paramsDirect.quality = 1000;
	paramsDirect.applyCurrentSolution = false;
	paramsDirect.applyEnvironment = true;
	paramsDirect.applyLights = true;
	rr::RRDynamicSolver::UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = true;
	paramsIndirect.applyLights = true;
	solver->updateLightmaps(layerNumber,true,&paramsDirect,&paramsIndirect); 
}

void saveIlluminationToDisk(rr::RRDynamicSolver* solver, unsigned layerNumber)
{
	for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
	{
		char filename[1000];

		// save vertex buffer
		rr::RRIlluminationVertexBuffer* vbuf = solver->getIllumination(objectIndex)->getLayer(layerNumber)->vertexBuffer;
		if(vbuf)
		{
			sprintf(filename,"../../data/export/%d.vbu",objectIndex );
			bool saved = vbuf->save(filename);
			printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
		}

		// save pixel buffer
		rr::RRIlluminationPixelBuffer* map = solver->getIllumination(objectIndex)->getLayer(layerNumber)->pixelBuffer;
		if(map)
		{
			sprintf(filename,"../../data/export/%d.png",objectIndex );
			bool saved = map->save(filename);
			printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
		}
	}
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

	// decrease priority, so that this task runs on background using only free CPU cycles
	// good for precalculating lightmaps on workstation
	SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);

	// load uniform white environment from 6 1x1 textures
	//const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	//rr::RRIlluminationEnvironmentMap* environmentMap = rr::RRIlluminationEnvironmentMap::load("..\\..\\data\\maps\\whitebox\\whitebox_%s.png",cubeSideNames,true,true);
	// create uniform white environment
	rr::RRIlluminationEnvironmentMap* environmentMap = rr::RRIlluminationEnvironmentMap::createUniform();

	// init scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	rr::RRDynamicSolver* solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	rr::RRStaticSolver::SmoothingParameters smoothingParams;
	smoothingParams.stitchDistance = 0;
	smoothingParams.minFeatureSize = 0;

	FCDocument* collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("../../data/scenes/koupelna/koupelna4-windows.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	solver->setObjects( *adaptObjectsFromFCollada( collada ), &smoothingParams );
	solver->setLights( *adaptLightsFromFCollada( collada ) );
	solver->setEnvironment( environmentMap );

	time_t seconds = time(NULL);

	solver->calculate();
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// calculate and save it
	calculatePerVertexAndSelectedPerPixel(solver,0); // calculatePerPixel(solver,0);
	saveIlluminationToDisk(solver,0);

	time_t newseconds = time(NULL) - seconds;
	printf("time taken %d seconds \n", newseconds );

	printf("\nPress any key to close...");
	fgetc(stdin);
	return 0;
}
