// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "PreserveState.h"

#define BIG_MAP_SIZEX            1024 // size of temporary texture used during detection
#define BIG_MAP_SIZEY            1024 // set 1200 to process 70k triangle Sponza in 1 pass
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
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%triCountX)+((vertexIndex==2)?1:0)-triCountX*0.5f+0.05f)/(triCountX*0.5f);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/triCountX)+((vertexIndex==0)?1:0)-triCountY*0.5f+0.05f)/(triCountY*0.5f);
		// +0.05f makes triangle area larger [in 4x4, from 6 to 10 pixels]
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

RRDynamicSolverGL::RRDynamicSolverGL(const char* _pathToShaders, DDIQuality _detectionQuality)
{
	strncpy(pathToShaders,_pathToShaders,299);
	pathToShaders[299]=0;

	switch(_detectionQuality)
	{
		case DDI_4X4:
		case DDI_8X8:
			detectionQuality = _detectionQuality;
			break;
		default:
		{
			// known to driver-crash with 8x8: X300
			// known to work with 8x8: X1600, X2400, 6150, 7100, 8800
			// so our automatic pick is:
			//   8x8 for: gf6000..9999, radeon 1300..4999
			//   4x4 for others
			detectionQuality = DDI_4X4;
			char* renderer = (char*)glGetString(GL_RENDERER);
			if(renderer)
			{
				// find 4digit number
				unsigned number = 0;
				#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
				for(unsigned i=0;renderer[i];i++)
					if(!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
					{
						number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
						break;
					}

				if( ((strstr(renderer,"Radeon")||strstr(renderer,"RADEON")) && (number>=1300 && number<=4999)) ||
					((strstr(renderer,"GeForce")||strstr(renderer,"GEFORCE")) && (number>=6000 && number<=9999)) )
						detectionQuality = DDI_8X8;
			}
			break;
		}
	}
	unsigned faceSizeX = (detectionQuality==DDI_8X8)?8:4;
	unsigned faceSizeY = (detectionQuality==DDI_8X8)?8:4;
	rr::RRReporter::report(rr::INF2,"Detection quality: %s%s.\n",(_detectionQuality==DDI_AUTO)?"auto->":"",(detectionQuality==DDI_4X4)?"low":"high");

	captureUv = new CaptureUv;
	detectBigMap = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,BIG_MAP_SIZEX,BIG_MAP_SIZEY,1,rr::BF_RGBA,true,NULL),false,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%sscaledown_filter.vs",pathToShaders);
	_snprintf(buf2,399,"%sscaledown_filter.fs",pathToShaders);
	char buf3[100];
	sprintf(buf3,"#define SIZEX %d\n#define SIZEY %d\n",faceSizeX,faceSizeY);
	scaleDownProgram = Program::create(buf3,buf1,buf2);
	if(!scaleDownProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %sscaledown_filter.*\n",pathToShaders);

	rendererNonCaching = NULL;
	rendererCaching = NULL;
	rendererObject = NULL;
	detectedDirectSum = NULL;
	detectedNumTriangles = 0;
	observer = NULL;
	oldObserverPos = rr::RRVec3(1e6);
	honourExpensiveLightingShadowingFlags = false;

	_snprintf(buf1,399,"%subershader.vs",pathToShaders);
	_snprintf(buf2,399,"%subershader.fs",pathToShaders);
	uberProgram1 = UberProgram::create(buf1,buf2);
}

RRDynamicSolverGL::~RRDynamicSolverGL()
{
	for(unsigned i=0;i<realtimeLights.size();i++) delete realtimeLights[i];
	delete[] detectedDirectSum;
	delete rendererCaching;
	delete rendererNonCaching;
	delete scaleDownProgram;
	if(detectBigMap) delete detectBigMap->getBuffer();
	delete detectBigMap;
	delete captureUv;
	delete uberProgram1;
}

void RRDynamicSolverGL::calculate(CalculateParameters* params)
{
	// after camera change, dirty travelling lights
	if(observer && observer->pos!=oldObserverPos)
	{
		//rr::RRReporter::report(rr::INF2,"Dirty directional lights.\n");
		oldObserverPos = observer->pos;
		for(unsigned i=0;i<realtimeLights.size();i++)
			if(realtimeLights[i]->getParent()->orthogonal && realtimeLights[i]->getNumInstances())
			{
				realtimeLights[i]->dirty = true;
				reportDirectIlluminationChange(true);
			}
	}

	RRDynamicSolver::calculate(params);
}

void RRDynamicSolverGL::setLights(const rr::RRLights& _lights)
{
	// create realtime lights
	RRDynamicSolver::setLights(_lights);
	for(unsigned i=0;i<realtimeLights.size();i++) delete realtimeLights[i];
	realtimeLights.clear();
	for(unsigned i=0;i<_lights.size();i++) realtimeLights.push_back(new rr_gl::RealtimeLight(*_lights[i]));
	// reset detected direct lighting
	if(detectedDirectSum) memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
}

void RRDynamicSolverGL::updateDirtyLights()
{
	PreserveViewport p1;
	PreserveMatrices p2;

	if(!getMultiObjectCustom()) return;

	// alloc space for detected direct illum
	unsigned updated = 0;
	unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
	if(numTriangles!=detectedNumTriangles)
	{
		delete[] detectedDirectSum;
		detectedNumTriangles = numTriangles;
		detectedDirectSum = new unsigned[detectedNumTriangles];
		memset(detectedDirectSum,0,detectedNumTriangles*sizeof(unsigned));
	}

	// update shadowmaps and smallMapsCPU
	for(unsigned i=0;i<realtimeLights.size();i++)
	{
		REPORT(rr::RRReportInterval report(rr::INF3,"Updating shadowmap...\n"));
		RealtimeLight* light = realtimeLights[i];
		if(!light)
		{
			RR_ASSERT(0);
			continue;
		}
		if(numTriangles!=light->numTriangles)
		{
			delete[] light->smallMapCPU;
			light->numTriangles = numTriangles;
			light->smallMapCPU = new unsigned[numTriangles+2047]; // allocate more so we can read complete last line from GPU, including unused values
			light->dirty = 1;
		}
		if(!light->dirty) continue;
		light->dirty = 0;
		// update dirlight position
		if(light->getParent()->orthogonal && light->getNumInstances())
		{
			Texture* shadowmap = light->getShadowMap(0);
			light->getParent()->update(observer,MIN(shadowmap->getBuffer()->getWidth(),shadowmap->getBuffer()->getHeight()));
		}
		else
			light->getParent()->update(NULL,0);
		// sync RRLight pos/dir with realtimeLight pos/dir
		// (not necessary for us, but user might want to calculate offline later and he would be surprised, that RRLight is at old position while he moved realtimeLight)
		if(getLights().size()>i && getLights()[i])
		{
			getLights()[i]->position = light->getParent()->pos;
			getLights()[i]->direction = light->getParent()->dir;
		}
		// update shadowmap[s]
		{
			glClearDepth(0.9999); // prevents backprojection
			glColorMask(0,0,0,0);
			glEnable(GL_POLYGON_OFFSET_FILL);
			rr_gl::UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
			for(unsigned i=0;i<light->getNumInstances();i++)
			{
				Camera* lightInstance = light->getInstance(i);
				lightInstance->setupForRender();
				delete lightInstance;
				Texture* shadowmap = light->getShadowMap(i);
				glViewport(0, 0, shadowmap->getBuffer()->getWidth(), shadowmap->getBuffer()->getHeight());
				shadowmap->renderingToBegin();
				glClear(GL_DEPTH_BUFFER_BIT);
				renderScene(uberProgramSetup,light->origin);
			}
			glDisable(GL_POLYGON_OFFSET_FILL);
			glColorMask(1,1,1,1);
			if(light->getNumInstances())
				light->getShadowMap(0)->renderingToEnd();
		}
		// update smallmap
		setupShaderLight = light;
		updated += detectDirectIlluminationTo(light->smallMapCPU,light->numTriangles);
	}

	// sum smallMapsCPU into detectedDirectSum
	if(updated && realtimeLights.size()>1)
	{
		unsigned numLights = realtimeLights.size();
#pragma omp parallel for
		for(int b=0;b<(int)numTriangles*4;b++)
		{
			unsigned sum = 0;
			for(unsigned l=0;l<numLights;l++)
				sum += ((unsigned char*)realtimeLights[l]->smallMapCPU)[b];
			((unsigned char*)detectedDirectSum)[b] = CLAMPED(sum,0,255);
		}
	}
}

unsigned* RRDynamicSolverGL::detectDirectIllumination()
{
	updateDirtyLights();
	if(realtimeLights.size()==1) return realtimeLights[0]->smallMapCPU;
	return detectedDirectSum;
}

void RRDynamicSolverGL::setupShader(unsigned objectNumber)
{
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = (setupShaderLight->areaType==RealtimeLight::POINT)?setupShaderLight->getNumInstances():(setupShaderLight->getNumInstances()?1:0);
	uberProgramSetup.SHADOW_SAMPLES = uberProgramSetup.SHADOW_MAPS?1:0;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_COLOR = setupShaderLight->origin && setupShaderLight->origin->color!=rr::RRVec3(1);
	uberProgramSetup.LIGHT_DIRECT_MAP = setupShaderLight->areaType!=rr_gl::RealtimeLight::POINT && uberProgramSetup.SHADOW_MAPS && setupShaderLight->lightDirectMap;
	uberProgramSetup.LIGHT_DIRECTIONAL = setupShaderLight->getParent()->orthogonal;
	uberProgramSetup.LIGHT_DISTANCE_PHYSICAL = setupShaderLight->origin && setupShaderLight->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
	uberProgramSetup.LIGHT_DISTANCE_POLYNOMIAL = setupShaderLight->origin && setupShaderLight->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
	uberProgramSetup.LIGHT_DISTANCE_EXPONENTIAL = setupShaderLight->origin && setupShaderLight->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.FORCE_2D_POSITION = true;
	if(!uberProgramSetup.useProgram(uberProgram1,setupShaderLight,0,NULL,1))
	{
		rr::RRReporter::report(rr::ERRO,"setupShader: Failed to compile or link GLSL program.\n");
	}
}

unsigned RRDynamicSolverGL::detectDirectIlluminationTo(unsigned* _results, unsigned _space)
{
	if(!scaleDownProgram || !_results)
	{
		RR_ASSERT(0);
		return 0;
	}

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
			rendererNonCaching = RendererOfRRObject::create(getMultiObjectCustom(),this,getScaler(),true);
			rendererCaching = rendererNonCaching->createDisplayList();
		}
	}
	if(!rendererObject) return 0;

	rr::RRMesh* mesh = getMultiObjectCustom()->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	if(!numTriangles)
	{
		RR_ASSERT(0); // legal, but should not happen in well coded program
		return 0;
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
	unsigned faceSizeX = (detectionQuality==DDI_8X8)?8:4;
	unsigned faceSizeY = (detectionQuality==DDI_8X8)?8:4;
	captureUv->triCountX = BIG_MAP_SIZEX/faceSizeX; // number of triangles in one row
	captureUv->triCountY = BIG_MAP_SIZEY/faceSizeY; // number of triangles in one column
	while(captureUv->triCountY>1 && numTriangles/(captureUv->triCountX*captureUv->triCountY)==numTriangles/(captureUv->triCountX*(captureUv->triCountY-1))) captureUv->triCountY--;
	unsigned width = BIG_MAP_SIZEX; // used width in pixels
	unsigned height = captureUv->triCountY*faceSizeY; // used height in pixels

	// for each set of triangles (if all triangles don't fit into one texture)
	for(captureUv->firstCapturedTriangle=0;captureUv->firstCapturedTriangle<numTriangles;captureUv->firstCapturedTriangle+=captureUv->triCountX*captureUv->triCountY)
	{
		captureUv->lastCapturedTrianglePlus1 = MIN(numTriangles,captureUv->firstCapturedTriangle+captureUv->triCountX*captureUv->triCountY);

		// prepare for scaling down -> render to texture
		detectBigMap->renderingToBegin();

		// clear
		glViewport(0, 0, width,height);
		glClear(GL_COLOR_BUFFER_BIT); // old pixels are overwritten, so clear is usually not needed, but individual light-triangle lighting may be disabled by getTriangleMaterial()=triangles are not rendered, and we need to detect 0 rather than uninitialized value

		// setup renderer
		RendererOfRRObject::RenderedChannels renderedChannels;
		renderedChannels.NORMALS = true;
		renderedChannels.LIGHT_DIRECT = true;
		renderedChannels.FORCE_2D_POSITION = true;

		// setup shader
		setupShader(0);

		// render scene
		rendererNonCaching->setRenderedChannels(renderedChannels);
		rendererNonCaching->setCapture(captureUv,captureUv->firstCapturedTriangle,captureUv->lastCapturedTrianglePlus1); // set param for cache so it creates different displaylists
		rendererNonCaching->setLightingShadowingFlags(NULL,setupShaderLight->origin,honourExpensiveLightingShadowingFlags);
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
		glViewport(0,0,BIG_MAP_SIZEX/faceSizeX,BIG_MAP_SIZEY/faceSizeY);//!!! needs at least 256x256 backbuffer
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
		RR_ASSERT(captureUv->triCountX*captureUv->triCountY <= BIG_MAP_SIZEX*BIG_MAP_SIZEY/faceSizeX/faceSizeY);
		REPORT(rr::RRReportInterval report(rr::INF3,"glReadPix %dx%d\n", captureUv->triCountX, captureUv->triCountY));
		RR_ASSERT(_space+2047>=captureUv->firstCapturedTriangle+captureUv->triCountX*captureUv->triCountY);
		glReadPixels(0, 0, captureUv->triCountX, captureUv->triCountY, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, _results+captureUv->firstCapturedTriangle);
	}

	return 1;
}


void drawCamera(Camera* camera)
{
	if(camera)
	{
		glPushMatrix();
		glMultMatrixd(camera->inverseViewMatrix);
		glMultMatrixd(camera->inverseFrustumMatrix);
		glBegin(GL_LINE_STRIP);
		glColor3f(0,0,0);
		glVertex3f(1,1,1);
		glColor3f(1,1,1);
		glVertex3f(1,1,-1);
		glVertex3f(-1,1,-1);
		glColor3f(0,0,0);
		glVertex3f(-1,1,1);
		glVertex3f(1,1,1);
		glVertex3f(1,-1,1);
		glVertex3f(-1,-1,1);
		glColor3f(1,1,1);
		glVertex3f(-1,-1,-1);
		glVertex3f(1,-1,-1);
		glColor3f(0,0,0);
		glVertex3f(1,-1,1);
		glEnd();
		glBegin(GL_LINES);
		glColor3f(1,1,1);
		glVertex3f(1,1,-1);
		glVertex3f(1,-1,-1);
		glVertex3f(-1,1,-1);
		glVertex3f(-1,-1,-1);
		glColor3f(0,0,0);
		glVertex3f(-1,1,1);
		glVertex3f(-1,-1,1);
		glEnd();
		glPopMatrix();
	}
}

void drawRealtimeLight(RealtimeLight* light)
{
	if(light)
		for(unsigned i=0;i<light->getNumInstances();i++)
		{
			Camera* camera = light->getInstance(i);
			drawCamera(camera);
			delete camera;
		}
}

void RRDynamicSolverGL::renderLights()
{
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 1;
	uberProgramSetup.MATERIAL_DIFFUSE = 1;
	Program* program = uberProgramSetup.useProgram(uberProgram1,NULL,0,NULL,1);
	for(unsigned i=0;i<getLights().size();i++)
	{
		drawRealtimeLight(realtimeLights[i]);
	}
}

unsigned RRDynamicSolverGL::updateEnvironmentMap(rr::RRObjectIllumination* illumination)
{
	unsigned updated = rr::RRDynamicSolver::updateEnvironmentMap(illumination);
	if(illumination->diffuseEnvMap)
		getTexture(illumination->diffuseEnvMap,false)->reset(false);
	if(illumination->specularEnvMap)
		getTexture(illumination->specularEnvMap,false)->reset(false);
	return updated;
}

}; // namespace
