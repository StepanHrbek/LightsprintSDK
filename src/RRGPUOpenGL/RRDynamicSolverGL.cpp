// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdarg>
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h"

#define BIG_MAP_SIZE            1024 // size of temporary texture used during detection

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
	detectBigMap = de::Texture::create(NULL,BIG_MAP_SIZE,BIG_MAP_SIZE,false,de::Texture::TF_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
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
	boostDetectedDirectIllumination = 1;

}

RRDynamicSolverGL::~RRDynamicSolverGL()
{
	delete rendererCaching;
	delete rendererNonCaching;
	delete scaleDownProgram;
	delete[] detectSmallMap;
	delete detectBigMap;
	delete captureUv;

}


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
		//glClear(GL_COLOR_BUFFER_BIT); // not necessary, old pixels should be overwritten

		// setup renderer
		RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.NORMALS = true;
		renderedChannels.LIGHT_DIRECT = true;
		renderedChannels.FORCE_2D_POSITION = true;

		// setup shader
		{
			// standard path customizable by subclassing
			//!!! no support for per object shaders yet
			setupShader(0);
		}

		// render scene
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setCapture(captureUv,captureUv->firstCapturedTriangle,captureUv->lastCapturedTrianglePlus1); // set param for cache so it creates different displaylists
		if(renderedChannels.LIGHT_INDIRECT_MAP)
			rendererNonCaching->render();
		else
			rendererCaching->render();
		rendererNonCaching->setCapture(NULL,0,numTriangles);

		// downscale 10pixel triangles in 4x4 squares to single pixel values
		detectBigMap->renderingToEnd();
		scaleDownProgram->useIt();
		scaleDownProgram->sendUniform("lightmap",0);
		scaleDownProgram->sendUniform("pixelDistance",1.0f/BIG_MAP_SIZE,1.0f/BIG_MAP_SIZE);
		glViewport(0,0,BIG_MAP_SIZE/4,BIG_MAP_SIZE/4);//!!! needs at least 256x256 backbuffer
		// clear to alpha=0 (color=pink, if we see it in scene, filtering or uv mapping is wrong)
		//glClearColor(1,0,1,0);
		//glClear(GL_COLOR_BUFFER_BIT);
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
		//glClearColor(0,0,0,0);
		glEnable(GL_CULL_FACE);

		// read downscaled image to memory
		glReadPixels(0, 0, captureUv->triCountX, captureUv->triCountY, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, detectSmallMap);

		// send triangle irradiances to solver
		const float boost = boostDetectedDirectIllumination/255;
#pragma omp parallel for schedule(static)
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
