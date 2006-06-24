//
// OpenGL renderer of RRObject by Stepan Hrbek, dee@mail.cz
//

#include "rr2gl.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <assert.h>
#include <math.h>


int   SIDES  =1; // 1,2=force all faces 1/2-sided, 0=let them as specified by surface
bool  SMOOTH =1; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)
bool  COMPILE=1;

#define MINUS(a,b,res) res[0]=a[0]-b[0];res[1]=a[1]-b[1];res[2]=a[2]-b[2]
#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define DIV(a,b,res) res[0]=a[0]/(b);res[1]=a[1]/(b);res[2]=a[2]/(b)
#define MIN(a,b) (((a)<(b))?(a):(b))

VertexDataGenerator* generateForcedUv = NULL;

void checkGlError();


//////////////////////////////////////////////////////////////////////////////
//
// RRRenderer

const void* RRRenderer::getParams(unsigned& length) const
{
	length = 0;
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRGLObjectRenderer

RRGLObjectRenderer::RRGLObjectRenderer(rr::RRObject* objectImporter, rr::RRScene* radiositySolver)
{
	params.object = objectImporter;
	params.scene = radiositySolver;
}

void RRGLObjectRenderer::setChannel(ColorChannel cc)
{
	params.cc = cc;
}

const void* RRGLObjectRenderer::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RRGLObjectRenderer::render()
{
	glColor4ub(0,0,0,255);
	//glEnable(GL_CULL_FACE);
	if(SIDES==1) glEnable(GL_CULL_FACE);
	if(SIDES==2) glDisable(GL_CULL_FACE);

	switch(params.cc)
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
		rr::RRScene::setState(rr::RRScene::GET_SOURCE,1);
		rr::RRScene::setState(rr::RRScene::GET_REFLECTED,0);
		glShadeModel(GL_SMOOTH);
		break;
	case CC_REFLECTED_IRRADIANCE:
	case CC_REFLECTED_EXITANCE:
		rr::RRScene::setState(rr::RRScene::GET_SOURCE,0);
		rr::RRScene::setState(rr::RRScene::GET_REFLECTED,1);
		glShadeModel(GL_SMOOTH);
		break;
	default:
		assert(0);
	}

	checkGlError();
	glBegin(GL_TRIANGLES);
	assert(params.object);
	rr::RRMesh* meshImporter = params.object->getCollider()->getMesh();
	unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
	for(unsigned triangleIdx=0;triangleIdx<numTriangles;triangleIdx++)
	{
		rr::RRMesh::Triangle tri;
		meshImporter->getTriangle(triangleIdx,tri);
		switch(params.cc)
		{
			case CC_NO_COLOR:
				break;
			case CC_TRIANGLE_INDEX:
				glColor4ub(triangleIdx>>16,triangleIdx>>8,triangleIdx,255);
				break;
			default:
			{
				unsigned surfaceIdx = params.object->getTriangleSurface(triangleIdx);
				if(surfaceIdx!=oldSurfaceIdx)
				{
					const rr::RRSurface* surface = params.object->getSurface(surfaceIdx);
					assert(surface);
					// nastavuje culling podle materialu
					// vypnuto protoze kdyz to na nvidii vlozim do display listu, pri jeho provadeni hlasi error
					//if(cc!=CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION)
					//	if((SIDES==0 && surface->sideBits[1].renderFrom) || SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
					switch(params.cc)
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
		rr::RRObject::TriangleNormals triangleNormals;
		bool setNormals = params.cc!=CC_NO_COLOR && params.cc!=CC_TRIANGLE_INDEX;
		if(setNormals)
		{
			params.object->getTriangleNormals(triangleIdx,triangleNormals);
		}
		for(int v=0;v<3;v++) 
		{
			if(setNormals && (SMOOTH || v==0))
			{
				glNormal3fv(&triangleNormals.norm[v].x);
			}
			rr::RRMesh::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);

			switch(params.cc)
			{
				case CC_SOURCE_IRRADIANCE:
				case CC_REFLECTED_IRRADIANCE:
					{
					rr::RRColor color;
					params.scene->getTriangleMeasure(0,triangleIdx,v,rr::RM_IRRADIANCE,color);
					glColor3fv(&color.x);
					break;
					}
				case CC_SOURCE_EXITANCE:
				case CC_REFLECTED_EXITANCE:
					{
					rr::RRColor color;
					params.scene->getTriangleMeasure(0,triangleIdx,v,rr::RM_EXITANCE,color);
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
// RRGLCachingRenderer

RRGLCachingRenderer::RRGLCachingRenderer(RRRenderer* arenderer)
{
	renderer = arenderer;
}

RRGLCachingRenderer::~RRGLCachingRenderer()
{
	for(Map::iterator i=mapa.begin();i!=mapa.end();i++)
	{
		setStatus(CS_NEVER_COMPILE,i->second);
	}
}

RRGLCachingRenderer::Info& RRGLCachingRenderer::findInfo()
{
	Key key;
	memset(&key,0,sizeof(key));
	unsigned length;
	const void* params = renderer->getParams(length);
	if(length)
	{
		//!!! params delsi nez 16 jsou oriznuty
		memcpy(&key,params,MIN(length,sizeof(key)));
	}
//	Map::iterator i = mapa.find(key);
//	if(i!=mapa.end())
//		return (Info&)i->second;
	return mapa[key];
}

void RRGLCachingRenderer::setStatus(ChannelStatus cs,RRGLCachingRenderer::Info& info)
{
	if(info.status==CS_COMPILED && cs!=CS_COMPILED)
	{
		assert(info.displayList!=UINT_MAX);
		glDeleteLists(info.displayList,1);
		info.displayList = UINT_MAX;
	}
	if(info.status!=CS_COMPILED && cs==CS_COMPILED)
	{
		assert(info.displayList==UINT_MAX);
		cs = CS_READY_TO_COMPILE;
	}
	info.status = cs;
}

void RRGLCachingRenderer::setStatus(ChannelStatus cs)
{
	setStatus(cs,findInfo());
}

void RRGLCachingRenderer::render()
{
	RRGLCachingRenderer::Info& info = findInfo();
	switch(info.status)
	{
	case CS_READY_TO_COMPILE:
		if(!COMPILE) goto never;
		assert(info.displayList==UINT_MAX);
		info.displayList = glGenLists(1);
		glNewList(info.displayList,GL_COMPILE);
		renderer->render();
		glEndList();
		info.status = CS_COMPILED;
		// intentionally no break
	case CS_COMPILED:
		assert(info.displayList!=UINT_MAX);
		glCallList(info.displayList);
		break;
	case CS_NEVER_COMPILE:
never:
		assert(info.displayList==UINT_MAX);
		renderer->render();
		break;
	default:
		assert(0);
	}
}
