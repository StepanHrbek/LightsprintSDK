//
// OpenGL renderer of RRObjectImporter by Stepan Hrbek, dee@mail.cz
//

#include <GL/glew.h>
#include <GL/glut.h>
#include <assert.h>
#include <math.h>
#include "rr2gl.h"


int   SIDES  =1; // 1,2=force all faces 1/2-sided, 0=let them as specified by mgf
bool  NORMALS=0; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)
bool  COMPILE=1;

#define MINUS(a,b,res) res[0]=a[0]-b[0];res[1]=a[1]-b[1];res[2]=a[2]-b[2]
#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define DIV(a,b,res) res[0]=a[0]/(b);res[1]=a[1]/(b);res[2]=a[2]/(b)

VertexDataGenerator* generateForcedUv = NULL;

void checkGlError();

//////////////////////////////////////////////////////////////////////////////
//
// RRGLObjectRenderer


RRGLObjectRenderer::RRGLObjectRenderer(rrVision::RRObjectImporter* objectImporter, rrVision::RRScene* radiositySolver)
{
	object = objectImporter;
	scene = radiositySolver;
}

void RRGLObjectRenderer::render(ColorChannel cc)
{
	glColor4ub(0,0,0,255);
	glEnable(GL_CULL_FACE);

	switch(cc)
	{
	case CC_NO_COLOR: 
	case CC_TRIANGLE_INDEX: 
		glShadeModel(GL_FLAT);
		break;
	case CC_DIFFUSE_REFLECTANCE:
		glShadeModel(GL_SMOOTH);
		break;
	case CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
		glShadeModel(GL_SMOOTH);
		glDisable(GL_CULL_FACE);
		break;
	case CC_SOURCE_IRRADIANCE:
	case CC_SOURCE_EXITANCE:
		rrVision::RRScene::SetState(rrVision::RRScene::GET_SOURCE,1);
		rrVision::RRScene::SetState(rrVision::RRScene::GET_REFLECTED,0);
		glShadeModel(GL_SMOOTH);
		break;
	case CC_REFLECTED_IRRADIANCE:
	case CC_REFLECTED_EXITANCE:
		rrVision::RRScene::SetState(rrVision::RRScene::GET_SOURCE,0);
		rrVision::RRScene::SetState(rrVision::RRScene::GET_REFLECTED,1);
		glShadeModel(GL_SMOOTH);
		break;
	default:
		assert(0);
	}

	checkGlError();
	glBegin(GL_TRIANGLES);
	assert(object);
	rrCollider::RRMeshImporter* meshImporter = object->getCollider()->getImporter();
	unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
	for(unsigned triangleIdx=0;triangleIdx<numTriangles;triangleIdx++)
	{
		rrCollider::RRMeshImporter::Triangle tri;
		meshImporter->getTriangle(triangleIdx,tri);
		switch(cc)
		{
			case CC_NO_COLOR:
				break;
			case CC_TRIANGLE_INDEX:
				glColor4ub(triangleIdx>>16,triangleIdx>>8,triangleIdx,255);
				break;
			default:
			{
				unsigned surfaceIdx = object->getTriangleSurface(triangleIdx);
				if(surfaceIdx!=oldSurfaceIdx)
				{
					const rrVision::RRSurface* surface = object->getSurface(surfaceIdx);
					assert(surface);
					if(cc!=CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION)
						if((SIDES==0 && surface->sideBits[1].renderFrom) || SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
					switch(cc)
					{
						case CC_DIFFUSE_REFLECTANCE:
						case CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
							glColor3fv(&surface->diffuseReflectance.x);
							break;
						default:;
					}
					oldSurfaceIdx = surfaceIdx;
				}
			}
		}
		if(!NORMALS && cc!=CC_NO_COLOR && cc!=CC_TRIANGLE_INDEX)
		{
			rrVision::RRObjectImporter::TriangleNormals triangleNormals;
			object->getTriangleNormals(triangleIdx,triangleNormals);
			glNormal3fv(&triangleNormals.norm[0].x);
		}
		for(int v=0;v<3;v++) 
		{
			rrCollider::RRMeshImporter::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);

			switch(cc)
			{
				case CC_SOURCE_IRRADIANCE:
				case CC_REFLECTED_IRRADIANCE:
					{
					rrVision::RRColor color;
					scene->getTriangleMeasure(0,triangleIdx,v,rrVision::RM_IRRADIANCE,color);
					glColor3fv(&color.x);
					break;
					}
				case CC_SOURCE_EXITANCE:
				case CC_REFLECTED_EXITANCE:
					{
					rrVision::RRColor color;
					scene->getTriangleMeasure(0,triangleIdx,v,rrVision::RM_EXITANCE,color);
					glColor3fv(&color.x);
					break;
					}
				case CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
					if(generateForcedUv)
					{
						GLfloat xy[2];
						generateForcedUv->generateData(triangleIdx, v, xy, sizeof(xy));
						glMultiTexCoord2f(GL_TEXTURE7,xy[0],xy[1]);
					}
					break;
				default:;
			}

			glVertex3fv(&vertex.x);
		}
	}
	glEnd();
	checkGlError();
}


//////////////////////////////////////////////////////////////////////////////
//
// RRCachingRenderer

RRCachingRenderer::RRCachingRenderer(RRObjectRenderer* arenderer)
{
	renderer = arenderer;
	for(unsigned i=0;i<CC_LAST;i++)
	{
		status[i] = CS_READY_TO_COMPILE;
		displayLists[i] = UINT_MAX;
	}
}

RRCachingRenderer::~RRCachingRenderer()
{
	for(unsigned i=0;i<CC_LAST;i++)
	{
		setStatus((ColorChannel)i,CS_NEVER_COMPILE);
	}
}

void RRCachingRenderer::setStatus(ColorChannel cc, ChannelStatus cs)
{
	if(status[cc]==CS_COMPILED && cs!=CS_COMPILED)
	{
		assert(displayLists[cc]!=UINT_MAX);
		glDeleteLists(displayLists[cc],1);
		displayLists[cc] = UINT_MAX;
	}
	if(status[cc]!=CS_COMPILED && cs==CS_COMPILED)
	{
		assert(displayLists[cc]==UINT_MAX);
		cs = CS_READY_TO_COMPILE;
	}
	status[cc] = cs;
}

void RRCachingRenderer::render(ColorChannel cc)
{
	checkGlError();
	switch(status[cc])
	{
	case CS_READY_TO_COMPILE:
		if(!COMPILE) goto never;
		checkGlError();
		assert(displayLists[cc]==UINT_MAX);
		displayLists[cc] = glGenLists(1);
		glNewList(displayLists[cc],GL_COMPILE);
		renderer->render(cc);
		glEndList();
		status[cc] = CS_COMPILED;
		checkGlError();
		// intentionally no break
	case CS_COMPILED:
		assert(displayLists[cc]!=UINT_MAX);
		checkGlError();
		glCallList(displayLists[cc]);
		checkGlError();
		break;
	case CS_NEVER_COMPILE:
never:
		assert(displayLists[cc]==UINT_MAX);
		checkGlError();
		renderer->render(cc);
		checkGlError();
		break;
	default:
		assert(0);
	}
	checkGlError();
}
