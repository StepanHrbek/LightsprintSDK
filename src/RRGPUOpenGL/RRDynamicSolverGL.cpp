// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include <GL/glew.h>
#include "Lightsprint/RRGPUOpenGL.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h"

#define BOOST_DIRECT_ILLUM      2.0f
#define SCALE_DOWN_ON_GPU       // mnohem rychlejsi, ale zatim neovereny ze funguje vsude
//#define CAPTURE_TGA           // behem scale_down uklada mezivysledky do tga, pro rucni kontrolu
#define PRIMARY_SCAN_PRECISION  1 // 1nejrychlejsi/2/3nejpresnejsi, 3 s texturami nebude fungovat kvuli cachovani pokud se detekce vseho nevejde na jednu texturu - protoze displaylist myslim neuklada nastaveni textur
#define BIG_MAP_SIZE            1024

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// CaptureUv

//! Generates uv coords for capturing direct illumination into map with matrix of triangles.
class CaptureUv : public rr_gl::VertexDataGenerator
{
public:
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) // vertexIndex=0..2
	{
		RR_ASSERT(size==2*sizeof(GLfloat));
		RR_ASSERT(triangleIndex>=firstCapturedTriangle && triangleIndex<lastCapturedTrianglePlus1);
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%triCountX)+((vertexIndex==2)?1:0)-triCountX*0.5f+0.1f)/(triCountX*0.5f);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/triCountX)+((vertexIndex==0)?1:0)-triCountY*0.5f+0.1f)/(triCountY*0.5f);
		// +0.1f makes triangle area larger [in 4x4, from 6 to 10 pixels]
	}
	virtual unsigned getHash()
	{
		return firstCapturedTriangle+(triCountX<<8)+(triCountY<<16);
	}
	unsigned firstCapturedTriangle; // offset of first captured triangle in array of all triangles
	unsigned lastCapturedTrianglePlus1; // offset of last captured triangle + 1
	unsigned triCountX, triCountY; // number of columns/rows in 0..1,0..1 texture space (not necessarily used, some of them could be empty when detecting less than triCountX*triCountY triangles)
};

//! Generates uv coords for capturing direct illumination into lightmap (takes uv from RRObject).
class CaptureUvIntoLightmap : public rr_gl::VertexDataGenerator
{
public:
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) // vertexIndex=0..2
	{
		RR_ASSERT(size==sizeof(rr::RRVec2));
		rr::RRMesh::TriangleMapping tm;
		mesh->getTriangleMapping(triangleIndex,tm);
		*((rr::RRVec2*)vertexData) = (tm.uv[vertexIndex]-rr::RRVec2(0.5f))*2;
	}
	virtual unsigned getHash()
	{
		return (unsigned)(intptr_t)mesh;
	}
	rr::RRMesh* mesh;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolverGL

RRDynamicSolverGL::RRDynamicSolverGL(char* apathToShaders)
{
	strncpy(pathToShaders,apathToShaders,299);
	pathToShaders[299]=0;

	captureUv = new CaptureUv;
	detectBigMap = de::Texture::create(NULL,BIG_MAP_SIZE,BIG_MAP_SIZE,false,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
	smallMapSize = BIG_MAP_SIZE*BIG_MAP_SIZE
#ifdef SCALE_DOWN_ON_GPU
		/16
#endif
		;
	detectSmallMap = new unsigned[smallMapSize];
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%sscaledown_filter.vs",pathToShaders);
	_snprintf(buf2,399,"%sscaledown_filter.fs",pathToShaders);
	scaleDownProgram = de::Program::create(NULL,buf1,buf2);
	if(!scaleDownProgram) rr::RRReporter::report(rr::RRReporter::ERRO,"Helper shaders failed: %s/scaledown_filter.*\n",pathToShaders);

	rendererNonCaching = NULL;
	rendererCaching = NULL;

	// used by detectDirectIlluminationFromLightmaps
	_snprintf(buf1,399,"%subershader.vs",pathToShaders);
	_snprintf(buf2,399,"%subershader.fs",pathToShaders);
	detectFromLightmapUberProgram = new de::UberProgram(buf1,buf2);
	detectingFromLightmapLayer = -1;
}

RRDynamicSolverGL::~RRDynamicSolverGL()
{
	delete rendererCaching;
	delete rendererNonCaching;
	delete scaleDownProgram;
	delete[] detectSmallMap;
	delete detectBigMap;
	delete captureUv;

	// used by detectDirectIlluminationFromLightmaps
	delete detectFromLightmapUberProgram;
}

/*
per object lighting:
-renderovat po objektech je neprijemne, protoze to znamena vic facu (i degenerovany)
  -slo by zlepsit tim ze by byly degenrace zakazany a vstup s degeneraty zamitnut
-renderovat celou scenu a vyzobavat jen facy z objektu je neprijemne,
 protoze pak neni garantovane ze pujdou facy pekne za sebou
  -slo by zlepsit tim ze multiobjekt bude garantovat poradi(slo by pres zakaz optimalizaci v multiobjektu)
*/

bool RRDynamicSolverGL::detectDirectIllumination()
{
	if(!scaleDownProgram) return false;

	// update renderer after geometry change
	if(getMultiObjectCustom()!=rendererObject) // not equal? geometry must be changed
	{
		// delete old renderer
		SAFE_DELETE(rendererCaching);
		SAFE_DELETE(rendererNonCaching);
		// create new renderer
		// for empty scene, stay without renderer
		rendererObject = getMultiObjectCustom();
		if(rendererObject)
		{
			rendererNonCaching = new RendererOfRRObject(getMultiObjectCustom(),getStaticSolver(),getScaler(),true);
			rendererCaching = rendererNonCaching->createDisplayList();
		}
	}
	if(!rendererObject) return false;

	// backup render states
	glGetIntegerv(GL_VIEWPORT,viewport);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor);

	rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	if(!numTriangles)
	{
		RR_ASSERT(0); // legal, but should not happen in well coded program
		return true;
	}

	// adjust captured texture size so we don't waste pixels
	captureUv->triCountX = BIG_MAP_SIZE/4; // number of triangles in one row
	captureUv->triCountY = BIG_MAP_SIZE/4; // number of triangles in one column
	while(captureUv->triCountY>1 && numTriangles/(captureUv->triCountX*captureUv->triCountY)==numTriangles/(captureUv->triCountX*(captureUv->triCountY-1))) captureUv->triCountY--;
	unsigned width = BIG_MAP_SIZE; // used width in pixels
	unsigned height = captureUv->triCountY*4; // used height in pixels

	// setup render states
	glClearColor(0,0,0,1);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);

	// for each set of triangles (if all triangles don't fit into one texture)
	for(captureUv->firstCapturedTriangle=0;captureUv->firstCapturedTriangle<numTriangles;captureUv->firstCapturedTriangle+=captureUv->triCountX*captureUv->triCountY)
	{
		captureUv->lastCapturedTrianglePlus1 = MIN(numTriangles,captureUv->firstCapturedTriangle+captureUv->triCountX*captureUv->triCountY);

		// prepare for scaling down -> render to texture
		detectBigMap->renderingToBegin();

		// clear
		glViewport(0, 0, width,height);
		glClear(GL_COLOR_BUFFER_BIT);

		// setup renderer
		RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.LIGHT_DIRECT = true;
		renderedChannels.FORCE_2D_POSITION = true;

		// setup shader
		if(detectingFromLightmapLayer>=0)
		{
			// special path for detectDirectIlluminationFromLightmaps()
			// this will be removed in future
			renderedChannels.LIGHT_DIRECT = false;
			renderedChannels.LIGHT_INDIRECT_MAP = true;
			renderedChannels.LIGHT_MAP_LAYER = detectingFromLightmapLayer;
			de::UberProgramSetup detectFromLightmapUberProgramSetup;
			detectFromLightmapUberProgramSetup.LIGHT_INDIRECT_MAP = true;
			detectFromLightmapUberProgramSetup.MATERIAL_DIFFUSE = true;
			detectFromLightmapUberProgramSetup.FORCE_2D_POSITION = true;
			detectFromLightmapUberProgramSetup.useProgram(detectFromLightmapUberProgram,NULL,0,NULL,NULL,1);
		}
		else
		{
			// standard path customizable by subclassing
			//!!! no support for per object shaders yet
			setupShader(0);
		}

		// render scene
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setCapture(captureUv,captureUv->firstCapturedTriangle,captureUv->lastCapturedTrianglePlus1); // set param for cache so it creates different displaylists
		rendererCaching->render();
		//rendererNonCaching->render();
		rendererNonCaching->setCapture(NULL,0,numTriangles);

		// downscale 10pixel triangles in 4x4 squares to single pixel values
		detectBigMap->renderingToEnd();
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("lightmap",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/BIG_MAP_SIZE,1.0f/BIG_MAP_SIZE);
		glViewport(0,0,BIG_MAP_SIZE/4,BIG_MAP_SIZE/4);//!!! needs at least 256x256 backbuffer
		// clear to alpha=0 (color=pink, if we see it in scene, filtering or uv mapping is wrong)
		glClearColor(1,0,1,0);
		glClear(GL_COLOR_BUFFER_BIT);
		// setup pipeline
		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_CULL_FACE);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		detectBigMap->bindTexture();
		glBegin(GL_POLYGON);
		glMultiTexCoord2f(0,0,0);
		glVertex2f(-1,-1);
		glMultiTexCoord2f(0,0,1);
		glVertex2f(-1,1);
		glMultiTexCoord2f(0,1,1);
		glVertex2f(1,1);
		glMultiTexCoord2f(0,1,0);
		glVertex2f(1,-1);
		glEnd();
		glClearColor(0,0,0,0);
		glEnable(GL_CULL_FACE);

		// read downscaled image to memory
		glReadPixels(0, 0, captureUv->triCountX, captureUv->triCountY, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, detectSmallMap);

		// send triangle irradiances to solver
		float boost = BOOST_DIRECT_ILLUM / 255;
		#pragma omp parallel for
		for(int t=captureUv->firstCapturedTriangle;t<(int)captureUv->lastCapturedTrianglePlus1;t++)
		{
			unsigned triangleIndex = (unsigned)t;
			unsigned color = detectSmallMap[triangleIndex-captureUv->firstCapturedTriangle];
			getMultiObjectPhysicalWithIllumination()->setTriangleIllumination(
				triangleIndex,
				RM_IRRADIANCE_CUSTOM,
				rr::RRColor(((color>>24)&255)*boost,((color>>16)&255)*boost,((color>>8)&255)*boost));
		}
	}

	// restore render states
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]);
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);

	return true;
}

/*
// development version with additional features
bool RRDynamicSolverGL::detectDirectIllumination()
{
#ifdef SCALE_DOWN_ON_GPU
	if(!scaleDownProgram) return false;
#endif
	//printf("GL::detectDirectIllumination\n");
	//Timer w;w.Start();

	// first time init
	// this can't be done in constructor, because multiobject is not yet ready
	if(!rendererNonCaching)
	{
		if(!getMultiObjectCustom()) return false;
		rendererNonCaching = new RendererOfRRObject(getMultiObjectCustom(),getStaticSolver(),getScaler(),true);
		rendererCaching = rendererNonCaching->createDisplayList();
	}

	// backup render states
	glGetIntegerv(GL_VIEWPORT,viewport);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor);

	rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	if(!numTriangles)
	{
		RR_ASSERT(0); // legal, but should not happen in well coded program
		return true;
	}

	// adjust captured texture size so we don't waste pixels
	unsigned triSizeXRender = 4; // triSizeXRender = triangle width in pixels while rendering
	unsigned triSizeYRender = 4;
#ifdef SCALE_DOWN_ON_GPU
	captureUv->triCountX = BIG_MAP_SIZE/triSizeXRender; // triCountX = number of triangles in one row
	captureUv->triCountY = BIG_MAP_SIZE/triSizeYRender;
	unsigned triSizeXRead = 1; // triSizeXRead = triangle width in pixels while reading pixel results
	unsigned triSizeYRead = 1;
#else
	captureUv->triCountX = viewport[2]/triSizeXRender;
	captureUv->triCountY = viewport[3]/triSizeYRender;
	unsigned triSizeXRead = 4;
	unsigned triSizeYRead = 4;
#endif
	while(captureUv->triCountY>1 && numTriangles/(captureUv->triCountX*captureUv->triCountY)==numTriangles/(captureUv->triCountX*(captureUv->triCountY-1))) captureUv->triCountY--;

	// setup render states
	glClearColor(0,0,0,1);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);

#ifdef CAPTURE_TGA
	static int captured = 0;
	struct TgaHeader
	{
		TgaHeader(unsigned width, unsigned height)
		{
			pad[0]=pad[1]=pad[3]=pad[4]=pad[5]=pad[6]=pad[7]=pad[8]=pad[9]=pad[10]=pad[11]=0;
			pad[2]=2;
			pad[12]=width;
			pad[13]=width>>8;
			pad[14]=height;
			pad[15]=height>>8;
			pad[16]=32;
			pad[17]=40;
		}
		unsigned char pad[18];
	};
#endif

	// check that detectSmallMap is big enough for all pixels
	RR_ASSERT(captureUv->triCountX*triSizeXRead * captureUv->triCountY*triSizeYRead <= smallMapSize);
	//printf("%d %d\n",numTriangles,captureUv->triCountX*captureUv->triCountY);
	for(captureUv->firstCapturedTriangle=0;captureUv->firstCapturedTriangle<numTriangles;captureUv->firstCapturedTriangle+=captureUv->triCountX*captureUv->triCountY)
	{
		captureUv->lastCapturedTrianglePlus1 = MIN(numTriangles,captureUv->firstCapturedTriangle+captureUv->triCountX*captureUv->triCountY);

#ifdef SCALE_DOWN_ON_GPU
		// phase 1 for scale big map down
		detectBigMap->renderingToBegin();
#endif

		// clear
		glViewport(0, 0, captureUv->triCountX*triSizeXRender, captureUv->triCountY*triSizeYRender);
		glClear(GL_COLOR_BUFFER_BIT);

		// setup renderer
		RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.LIGHT_DIRECT = true;
#if PRIMARY_SCAN_PRECISION==2
		renderedChannels.MATERIAL_DIFFUSE_VCOLOR = true;
#endif
#if PRIMARY_SCAN_PRECISION==3
		renderedChannels.MATERIAL_DIFFUSE_MAP = true;
#endif
		renderedChannels.FORCE_2D_POSITION = true;

		// setup shader
		if(detectingFromLightmapLayer>=0)
		{
			// special path for detectDirectIlluminationFromLightmaps()
			renderedChannels.LIGHT_DIRECT = false;
			renderedChannels.LIGHT_INDIRECT_MAP = true;
			renderedChannels.LIGHT_MAP_LAYER = detectingFromLightmapLayer;
			de::UberProgramSetup detectFromLightmapUberProgramSetup;
			detectFromLightmapUberProgramSetup.LIGHT_INDIRECT_MAP = true;
			detectFromLightmapUberProgramSetup.MATERIAL_DIFFUSE = true;
			detectFromLightmapUberProgramSetup.FORCE_2D_POSITION = true;
			detectFromLightmapUberProgramSetup.useProgram(detectFromLightmapUberProgram,NULL,0,NULL,NULL,1);
		}
		else
		{
			// standard path customizable by subclassing
			//!!! no support for per object shaders yet
			setupShader(0);
		}

		// render scene
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setCapture(captureUv,captureUv->firstCapturedTriangle,captureUv->lastCapturedTrianglePlus1); // set param for cache so it creates different displaylists
		rendererCaching->render();
		//rendererNonCaching->render();
		rendererNonCaching->setCapture(NULL,0,numTriangles);

#ifdef CAPTURE_TGA
		if(captured>=0) {
			// read back the index buffer to memory
			unsigned pixels = captureUv->triCountX*triSizeXRender * captureUv->triCountY*triSizeYRender;
			unsigned* pixelBuffer = new unsigned[pixels];
			glReadPixels(0, 0, captureUv->triCountX*triSizeXRender, captureUv->triCountY*triSizeYRender, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);
			char fname[100];
			sprintf(fname,"h:\\cap_%d_hi.tga",captured);
			FILE* f=fopen(fname,"wb");
			TgaHeader t(captureUv->triCountX*triSizeXRender,captureUv->triCountY*triSizeYRender);
			fwrite(t.pad,18,1,f);
			//fwrite(pixelBuffer,4,pixels,f);
			for(unsigned i=0;i<pixels;i++) {fwrite((char*)(pixelBuffer+i)+1,1,1,f);fwrite((char*)(pixelBuffer+i)+2,1,1,f);fwrite((char*)(pixelBuffer+i)+3,1,1,f);fwrite((char*)(pixelBuffer+i)+0,1,1,f);}
			fclose(f);
			delete[] pixelBuffer;
		}
#endif

#ifdef SCALE_DOWN_ON_GPU
		// phase 2 for scale big map down
		detectBigMap->renderingToEnd();
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("lightmap",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/detectBigMap->getWidth(),1.0f/detectBigMap->getHeight());
		glViewport(0,0,captureUv->triCountX,BIG_MAP_SIZE/triSizeYRender);//!!! neni zarucene ze se vejde do backbufferu
		// clear to alpha=0 (color=pink, if we see it in scene, filtering or uv mapping is wrong)
		glClearColor(1,0,1,0);
		glClear(GL_COLOR_BUFFER_BIT);
		// setup pipeline
		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_CULL_FACE);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		detectBigMap->bindTexture();
		glBegin(GL_POLYGON);
		glMultiTexCoord2f(0,0,0);
		glVertex2f(-1,-1);
		glMultiTexCoord2f(0,0,1);
		glVertex2f(-1,1);
		glMultiTexCoord2f(0,1,1);
		glVertex2f(1,1);
		glMultiTexCoord2f(0,1,0);
		glVertex2f(1,-1);
		glEnd();
		glClearColor(0,0,0,0);
		glEnable(GL_CULL_FACE);
#endif

		// read back the index buffer to memory
		glReadPixels(0, 0, captureUv->triCountX*triSizeXRead, captureUv->triCountY*triSizeYRead, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, detectSmallMap);

#ifdef CAPTURE_TGA
		if(captured>=0) {
			// read back the index buffer to memory
			unsigned pixels = captureUv->triCountX*triSizeXRead * captureUv->triCountY*triSizeYRead;
			char fname[100];
			sprintf(fname,"h:\\cap_%d_lo.tga",captured);
			FILE* f=fopen(fname,"wb");
			TgaHeader t(captureUv->triCountX*triSizeXRead,captureUv->triCountY*triSizeYRead);
			fwrite(t.pad,18,1,f);
			//fwrite(pixelBuffer,4,pixels,f);
			for(unsigned i=0;i<pixels;i++) {fwrite((char*)(detectSmallMap+i)+1,1,1,f);fwrite((char*)(detectSmallMap+i)+2,1,1,f);fwrite((char*)(detectSmallMap+i)+3,1,1,f);fwrite((char*)(detectSmallMap+i)+0,1,1,f);}
			fclose(f);
			captured++;
		}
#endif

		// accumulate triangle irradiances
		rr::RRReal boost;
		if(triSizeXRead*triSizeYRead>1)
			boost = BOOST_DIRECT_ILLUM / (255*triSizeXRead*triSizeYRead/2); // compensate triangular area by /2
		else
			boost = BOOST_DIRECT_ILLUM / 255; // 1 pixel is square, no compensation
#pragma omp parallel for schedule(static,1)
		for(int triangleIndex=captureUv->firstCapturedTriangle;(unsigned)triangleIndex<captureUv->lastCapturedTrianglePlus1;triangleIndex++)
		{
			// accumulate 1 triangle power from square region in texture
			// (square coordinate calculation is in match with CaptureUv uv generator)
			unsigned sum[3] = {0,0,0};
			unsigned i = (triangleIndex-captureUv->firstCapturedTriangle)%captureUv->triCountX;
			unsigned j = (triangleIndex-captureUv->firstCapturedTriangle)/captureUv->triCountX;
			for(unsigned n=0;n<triSizeYRead;n++)
				for(unsigned m=0;m<triSizeXRead;m++)
				{
					unsigned pixel = captureUv->triCountX*triSizeXRead*(j*triSizeYRead+n) + (i*triSizeXRead+m);
					unsigned color = detectSmallMap[pixel] >> 8; // alpha was lost
					sum[0] += color>>16;
					sum[1] += (color>>8)&255;
					sum[2] += color&255;
				}
			// pass power to rrobject
			rr::RRColor avg = rr::RRColor(sum[0]*boost,sum[1]*boost,sum[2]*boost);
#if PRIMARY_SCAN_PRECISION==1
			getMultiObjectPhysicalWithIllumination()->setTriangleIllumination(triangleIndex,rr::RM_IRRADIANCE_CUSTOM,avg);
#else
			getMultiObjectPhysicalWithIllumination()->setTriangleIllumination(triangleIndex,rr::RM_EXITANCE_CUSTOM,avg);
#endif

		}
	}

#ifdef CAPTURE_TGA
	captured = -1;
#endif

	// restore render states
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]);
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);

	//printf("primary scan (%d-pass (%f)) took............ %d ms\n",numTriangles/(captureUv->triCountX*captureUv->triCountY)+1,(float)numTriangles/(captureUv->triCountX*captureUv->triCountY),(int)(1000*w.Watch()));
	return true;
};
*/

bool RRDynamicSolverGL::updateLightmap_GPU(unsigned objectIndex, rr::RRIlluminationPixelBuffer* lightmap)
{
	rr::RRObject* object = getObject(objectIndex);
	rr::RRMesh* mesh = object->getCollider()->getMesh();

	// prepare uv generator
	CaptureUvIntoLightmap captureUv;
	captureUv.mesh = mesh;

	// prepare render target
	lightmap->renderBegin();

	// setup shader
	setupShader(objectIndex);

	// prepare renderer
	// (could be cached later for higher speed)
	RendererOfRRObject* renderer = new RendererOfRRObject(object,getStaticSolver(),getScaler(),false);
	renderer->setCapture(&captureUv,0,mesh->getNumTriangles());
	RendererOfRRObject::RenderedChannels channels;
	channels.LIGHT_DIRECT = true;
	channels.FORCE_2D_POSITION = true;
	renderer->setRenderedChannels(channels);

	// render object
	// - all fragments must be rendered with alpha=1, renderEnd() needs it for correct filtering
	renderer->render();

	// cleanup renderer
	delete renderer;

	// restore render target
	lightmap->renderEnd(true);

	return true;
}

void RRDynamicSolverGL::detectDirectIlluminationFromLightmaps(unsigned sourceLayer)
{
	detectingFromLightmapLayer = sourceLayer;
	RRDynamicSolverGL::detectDirectIllumination();
	detectingFromLightmapLayer = -1;
}

}; // namespace
