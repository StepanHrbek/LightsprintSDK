// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "../DemoEngine/PreserveState.h"

#define BIG_MAP_SIZEX            1024 // size of temporary texture used during detection
#define BIG_MAP_SIZEY            1024 // set 1200 to process 70k triangle Sponza in 1 pass
#define FACE_SIZEX               8
#define FACE_SIZEY               8
#define REPORT(a) //a

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
		return (unsigned)(unsigned long long)mesh;
	}
	rr::RRMesh* mesh;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolverGL

RRDynamicSolverGL::RRDynamicSolverGL(char* _pathToShaders)
{
	strncpy(pathToShaders,_pathToShaders,299);
	pathToShaders[299]=0;

	captureUv = new CaptureUv;
	detectBigMap = Texture::create(NULL,BIG_MAP_SIZEX,BIG_MAP_SIZEY,false,Texture::TF_RGBA,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
	smallMapSize = BIG_MAP_SIZEX*BIG_MAP_SIZEY/FACE_SIZEX/FACE_SIZEY;
	detectSmallMap = new unsigned[smallMapSize*8]; // max static triangles = 64k*8 = 512k
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%sscaledown_filter.vs",pathToShaders);
	_snprintf(buf2,399,"%sscaledown_filter.fs",pathToShaders);
	char buf3[100];
	sprintf(buf3,"#define SIZEX %d\n#define SIZEY %d\n",FACE_SIZEX,FACE_SIZEY);
	scaleDownProgram = Program::create(buf3,buf1,buf2);
	if(!scaleDownProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %s/scaledown_filter.*\n",pathToShaders);

	rendererNonCaching = NULL;
	rendererCaching = NULL;

	rendererObject = NULL;

#ifdef RR_DEVELOPMENT
	// used by detectDirectIlluminationFromLightmaps
	_snprintf(buf1,399,"%subershader.vs",pathToShaders);
	_snprintf(buf2,399,"%subershader.fs",pathToShaders);
	detectFromLightmapUberProgram = UberProgram::create(buf1,buf2);
	detectingFromLightmapLayer = -1;
#endif
}

RRDynamicSolverGL::~RRDynamicSolverGL()
{
	delete rendererCaching;
	delete rendererNonCaching;
	delete scaleDownProgram;
	delete[] detectSmallMap;
	delete detectBigMap;
	delete captureUv;

#ifdef RR_DEVELOPMENT
	// used by detectDirectIlluminationFromLightmaps
	delete detectFromLightmapUberProgram;
#endif
}

#ifdef RR_DEVELOPMENT
/*
per object lighting:
-renderovat po objektech je neprijemne, protoze to znamena vic facu (i degenerovany)
  -slo by zlepsit tim ze by byly degenrace zakazany a vstup s degeneraty zamitnut
-renderovat celou scenu a vyzobavat jen facy z objektu je neprijemne,
 protoze pak neni garantovane ze pujdou facy pekne za sebou
  -slo by zlepsit tim ze multiobjekt bude garantovat poradi(slo by pres zakaz optimalizaci v multiobjektu)
*/
#endif

unsigned* RRDynamicSolverGL::detectDirectIllumination()
{
	if(!scaleDownProgram) return NULL;

	REPORT(rr::RRReportInterval report(rr::INF1,"detectDirectIllumination()\n"));

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

	rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	if(!numTriangles)
	{
		RR_ASSERT(0); // legal, but should not happen in well coded program
		return NULL;
	}

	// preserve render states, any changes will be restored at return from this function
	PreserveViewport p1;
	PreserveClearColor p2;
	PreserveCullFace p3;
	PreserveDepthMask p4;
	PreserveDepthTest p5;

	// setup render states
	glClearColor(0,0,0,1);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);

	// adjust captured texture size so we don't waste pixels
	captureUv->triCountX = BIG_MAP_SIZEX/FACE_SIZEX; // number of triangles in one row
	captureUv->triCountY = BIG_MAP_SIZEY/FACE_SIZEY; // number of triangles in one column
	while(captureUv->triCountY>1 && numTriangles/(captureUv->triCountX*captureUv->triCountY)==numTriangles/(captureUv->triCountX*(captureUv->triCountY-1))) captureUv->triCountY--;
	unsigned width = BIG_MAP_SIZEX; // used width in pixels
	unsigned height = captureUv->triCountY*FACE_SIZEY; // used height in pixels

	// for each set of triangles (if all triangles don't fit into one texture)
	for(captureUv->firstCapturedTriangle=0;captureUv->firstCapturedTriangle<numTriangles;captureUv->firstCapturedTriangle+=captureUv->triCountX*captureUv->triCountY)
	{
		captureUv->lastCapturedTrianglePlus1 = MIN(numTriangles,captureUv->firstCapturedTriangle+captureUv->triCountX*captureUv->triCountY);

		// prepare for scaling down -> render to texture
		detectBigMap->renderingToBegin();

		// clear
		glViewport(0, 0, width,height);
		//glClear(GL_COLOR_BUFFER_BIT); // not necessary, old pixels should be overwritten

		// setup renderer
		RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.NORMALS = true;
		renderedChannels.LIGHT_DIRECT = true;
		renderedChannels.FORCE_2D_POSITION = true;

		// setup shader
#ifdef RR_DEVELOPMENT
		if(detectingFromLightmapLayer>=0)
		{
			// special path for detectDirectIlluminationFromLightmaps()
			// this will be removed in future
			renderedChannels.LIGHT_DIRECT = false;
			renderedChannels.LIGHT_INDIRECT_MAP = true;
			UberProgramSetup detectFromLightmapUberProgramSetup;
			detectFromLightmapUberProgramSetup.LIGHT_INDIRECT_MAP = true;
			detectFromLightmapUberProgramSetup.MATERIAL_DIFFUSE = true;
			detectFromLightmapUberProgramSetup.FORCE_2D_POSITION = true;
			detectFromLightmapUberProgramSetup.useProgram(detectFromLightmapUberProgram,NULL,0,NULL,NULL,1);
			rendererNonCaching->setIndirectIlluminationLayer(detectingFromLightmapLayer);
		}
		else
#endif
		{
			// standard path customizable by subclassing
			//!!! no support for per object shaders yet
			setupShader(0);
		}

		// render scene
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setCapture(captureUv,captureUv->firstCapturedTriangle,captureUv->lastCapturedTrianglePlus1); // set param for cache so it creates different displaylists
		glDisable(GL_CULL_FACE);
		if(renderedChannels.LIGHT_INDIRECT_MAP)
			rendererNonCaching->render();
		else
			rendererCaching->render();
		rendererNonCaching->setCapture(NULL,0,numTriangles);

		// downscale 10pixel triangles in 4x4 squares to single pixel values
		detectBigMap->renderingToEnd();
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("lightmap",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/BIG_MAP_SIZEX,1.0f/BIG_MAP_SIZEY);
		glViewport(0,0,BIG_MAP_SIZEX/FACE_SIZEX,BIG_MAP_SIZEY/FACE_SIZEY);//!!! needs at least 256x256 backbuffer
		glActiveTexture(GL_TEXTURE0);
		detectBigMap->bindTexture();
		glDisable(GL_CULL_FACE);
		glBegin(GL_POLYGON);
			glMultiTexCoord2f(GL_TEXTURE0,0,0);
			glVertex2f(-1,-1);
			glMultiTexCoord2f(GL_TEXTURE0,0,1);
			glVertex2f(-1,1);
			glMultiTexCoord2f(GL_TEXTURE0,1,1);
			glVertex2f(1,1);
			glMultiTexCoord2f(GL_TEXTURE0,1,0);
			glVertex2f(1,-1);
		glEnd();

		// read downscaled image to memory
		RR_ASSERT(captureUv->triCountX*captureUv->triCountY<smallMapSize);
		REPORT(rr::RRReportInterval report(rr::INF3,"glReadPix\n"));
		glReadPixels(0, 0, captureUv->triCountX, captureUv->triCountY, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, detectSmallMap+captureUv->firstCapturedTriangle);
	}

	return detectSmallMap;
}

#ifdef RR_DEVELOPMENT

void RRDynamicSolverGL::detectDirectIlluminationFromLightmaps(unsigned sourceLayer)
{
	detectingFromLightmapLayer = sourceLayer;
	RRDynamicSolverGL::detectDirectIllumination(detectedCustomRGBA8);
	detectingFromLightmapLayer = -1;
}

unsigned RRDynamicSolverGL::updateVertexBuffersFromLightmaps(unsigned layerNumber, bool createMissingBuffers)
{
	// loads lightmap [per-pixel-custom] into multiobject [per-triangle-physical]
	detectDirectIlluminationFromLightmaps(layerNumber);
	// loads multiobject [per-triangle-physical] into solver [per triangle-physical]
//!!!	priv->scene->illuminationReset(false,true);
//!!!	priv->solutionVersion++;
	// reads interpolated solution [per-vertex-physical] from solver
	UpdateParameters params;
	params.measure = rr::RRRadiometricMeasure(0,0,0,1,0);
	return updateVertexBuffers(layerNumber,-1,createMissingBuffers,&params,NULL);
}
#endif

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
	channels.NORMALS = true;
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

/////////////////////////////////////////////////////////////////////////////
//
// save & load

char *bp(const char *fmt, ...)
{
	static char msg[1000];
	va_list argptr;
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	return msg;
}

unsigned RRDynamicSolverGL::loadIllumination(const char* path, unsigned layerNumber, bool vertexColors, bool lightmaps)
{
	unsigned result = 0;
	unsigned numObjects = getNumObjects();
	for(unsigned i=0;i<numObjects;i++)
	{
		rr::RRObjectIllumination* illumination = getIllumination(i);
		if(illumination)
		{
			rr::RRObjectIllumination::Layer* layer = illumination->getLayer(layerNumber);
			if(layer)
			{
				if(vertexColors)
				{
					delete layer->vertexBuffer;
					layer->vertexBuffer = rr::RRIlluminationVertexBuffer::load(bp("%svcol_%02d_%02d.vbu",path?path:"",i,layerNumber),illumination->getNumPreImportVertices());
					if(layer->vertexBuffer)
						result++;
				}
				if(lightmaps)
				{
					delete layer->pixelBuffer;
					layer->pixelBuffer = loadIlluminationPixelBuffer(bp("%slmap_%02d_%02d.png",path?path:"",i,layerNumber));
					if(layer->pixelBuffer)
						result++;
				}
			}
		}
	}
	return result;
}

unsigned RRDynamicSolverGL::saveIllumination(const char* path, unsigned layerNumber, bool vertexColors, bool lightmaps)
{
	unsigned result = 0;
	unsigned numObjects = getNumObjects();
	for(unsigned i=0;i<numObjects;i++)
	{
		rr::RRObjectIllumination* illumination = getIllumination(i);
		if(illumination)
		{
			rr::RRObjectIllumination::Layer* layer = illumination->getLayer(layerNumber);
			if(layer)
			{
				if(vertexColors && layer->vertexBuffer)
				{
					result += layer->vertexBuffer->save(bp("%svcol_%02d_%02d.vbu",path?path:"",i,layerNumber));
				}
				if(lightmaps && layer->pixelBuffer)
				{
					result += layer->pixelBuffer->save(bp("%slmap_%02d_%02d.png",path?path:"",i,layerNumber));
				}
			}
		}
	}
	return result;
}

}; // namespace
