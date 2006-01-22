//
// OpenGL renderer of RRObjectImporter by Stepan Hrbek, dee@mail.cz
//

#include <GL/glut.h>
#include "GL/glprocs.h"
#include <assert.h>
#include <math.h>
#include "rr2gl.h"


int   SIDES  =1; // 1,2=force all faces 1/2-sided, 0=let them as specified by mgf
bool  NORMALS=0; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)

//////////////////////////////////////////////////////////////////////////////
//
// rrobject input
//

rrVision::RRObjectImporter* objectImporter = NULL;
rrVision::RRScene* radiositySolver = NULL;

//////////////////////////////////////////////////////////////////////////////
//
// rrobject draw
//

#define MINUS(a,b,res) res[0]=a[0]-b[0];res[1]=a[1]-b[1];res[2]=a[2]-b[2]
#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define DIV(a,b,res) res[0]=a[0]/(b);res[1]=a[1]/(b);res[2]=a[2]/(b)

void rr2gl_draw_onlyz()
{
	if(SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

	rrCollider::RRMeshImporter* meshImporter = objectImporter->getCollider()->getImporter();

	glBegin(GL_TRIANGLES);
	unsigned numTriangles = meshImporter->getNumTriangles();
	for(unsigned i=0;i<numTriangles;i++)
	{
		rrCollider::RRMeshImporter::Triangle tri;
		meshImporter->getTriangle(i,tri);
		for(int v=0;v<3;v++) 
		{
			rrCollider::RRMeshImporter::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);
			glVertex3fv(&vertex.x);
		}
	}
	glEnd();
}

unsigned rr2gl_draw_indexed()
{
	static unsigned triangles;

	if(SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

	rrCollider::RRMeshImporter* meshImporter = objectImporter->getCollider()->getImporter();

	glColor4ub(0,0,0,255);

	glBegin(GL_TRIANGLES);
	triangles=0;
	unsigned numTriangles = meshImporter->getNumTriangles();
	for(unsigned i=0;i<numTriangles;i++)
	{
		rrCollider::RRMeshImporter::Triangle tri;
		meshImporter->getTriangle(i,tri);
		glColor4ub(triangles>>16,triangles>>8,triangles,255);
		triangles++;
		for(int v=0;v<3;v++) 
		{
			rrCollider::RRMeshImporter::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);
			glVertex3fv(&vertex.x);
		}
	}
	glEnd();
	return triangles;
}

void rr2gl_draw_colored(bool direct)
{
	glShadeModel(GL_SMOOTH);

	rrCollider::RRMeshImporter* meshImporter = objectImporter->getCollider()->getImporter();

	glColor4ub(0,0,0,255);

	glBegin(GL_TRIANGLES);
	unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
	for(unsigned triangleIdx=0;triangleIdx<numTriangles;triangleIdx++)
	{
		rrCollider::RRMeshImporter::Triangle tri;
		meshImporter->getTriangle(triangleIdx,tri);
		// kyz je to direct, color = barva materialu (bude se modulovat svetlem spocitanym v shaderu)
		if(direct)
		{
			unsigned surfaceIdx = objectImporter->getTriangleSurface(triangleIdx);
			if(surfaceIdx!=oldSurfaceIdx)
			{
				const rrVision::RRSurface* surface = objectImporter->getSurface(surfaceIdx);
				assert(surface);
				if((SIDES==0 && surface->sides==1) || SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
				glColor3fv(&surface->diffuseReflectanceColor.x);
				oldSurfaceIdx = surfaceIdx;
			}
			if(!NORMALS) 
			{
				rrVision::RRObjectImporter::TriangleNormals triangleNormals;
				objectImporter->getTriangleNormals(triangleIdx,triangleNormals);
				glNormal3fv(&triangleNormals.norm[0].x);
			}
		}
		for(int v=0;v<3;v++) 
		{
			rrCollider::RRMeshImporter::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);

			// kyz je to indirect, color = exitance, tj barva materialu uz vynasobena spocitanym svetlem, v shaderu se nic nemusi pocitat
			if(!direct && radiositySolver)
			{
				// mgf
				const rrVision::RRColor* indirect = radiositySolver->getTriangleRadiantExitance(0,triangleIdx,v);
				// 3ds
				//const rrVision::RRColor* indirect = radiositySolver->getTriangleIrradiance(0,triangleIdx,v);
				if(indirect)
				{
					glColor3fv(&indirect->x);
				}
			}

			glVertex3fv(&vertex.x);
		}
	}
	glEnd();
}


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
	objectImporter = object;
	radiositySolver = scene;
	switch(cc)
	{
	case CC_NO_COLOR: 
		rr2gl_draw_onlyz(); 
		break;
	case CC_TRIANGLE_INDEX: 
		rr2gl_draw_indexed(); 
		break;
	case CC_DIFFUSE_REFLECTANCE:
		rr2gl_draw_colored(true); 
		break;
	case CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION:
		rr2gl_draw_colored(true); 
		break;
		//		case CC_SOURCE_IRRADIANCE:
		//			rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,1);
		//			rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,0);
		//			rr2gl_draw_colored(false);
		//			break;
	case CC_SOURCE_EXITANCE:
		rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,1);
		rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,0);
		rr2gl_draw_colored(false);
		break;
		//		case CC_REFLECTED_IRRADIANCE:
		//			rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,0);
		//			rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,1);
		//			rr2gl_draw_colored(false);
		//			break;
	case CC_REFLECTED_EXITANCE:
		rrVision::RRSetState(rrVision::RRSS_GET_SOURCE,0);
		rrVision::RRSetState(rrVision::RRSS_GET_REFLECTED,1);
		rr2gl_draw_colored(false);
		break;
	default:
		assert(0);
	}
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
		displayLists[i] = -1;
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
		assert(displayLists[cc]!=-1);
		glDeleteLists(displayLists[cc],1);
		displayLists[cc] = -1;
	}
	if(status[cc]!=CS_COMPILED && cs==CS_COMPILED)
	{
		assert(displayLists[cc]==-1);
		cs = CS_READY_TO_COMPILE;
	}
	status[cc] = cs;
}

void RRCachingRenderer::render(ColorChannel cc)
{
	switch(status[cc])
	{
	case CS_READY_TO_COMPILE:
		assert(displayLists[cc]==-1);
		displayLists[cc] = glGenLists(1);
		glNewList(displayLists[cc],GL_COMPILE);
		renderer->render(cc);
		glEndList();
		status[cc] = CS_COMPILED;
		// intentionally no break
	case CS_COMPILED:
		assert(displayLists[cc]!=-1);
		glCallList(displayLists[cc]);
		break;
	case CS_NEVER_COMPILE:
		assert(displayLists[cc]==-1);
		renderer->render(cc);
		break;
	default:
		assert(0);
	}
}
