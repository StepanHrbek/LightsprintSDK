//
// OpenGL renderer of RRObjectImporter by Stepan Hrbek, dee@mail.cz
//

#include <GL/glut.h>
#include <GL/glprocs.h>
#include <assert.h>
#include <math.h>
#include "rr2gl.h"


int   SIDES  =1; // 1,2=force all faces 1/2-sided, 0=let them as specified by mgf
bool  NORMALS=0; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)
bool  COMPILE=1; // use display lists
bool  COMPILE_INDIRECT=1; // use display lists also for indirect

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

#define ONLYZ             10 // displaylists
#define INDEXED           11
#define COLORED_DIRECT    12
#define COLORED_INDIRECT  13

bool    rr2gl_compiled=false;

#define MINUS(a,b,res) res[0]=a[0]-b[0];res[1]=a[1]-b[1];res[2]=a[2]-b[2]
#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define DIV(a,b,res) res[0]=a[0]/(b);res[1]=a[1]/(b);res[2]=a[2]/(b)

void rr2gl_draw_onlyz()
{
	if(rr2gl_compiled) {glCallList(ONLYZ);return;}

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

	if(rr2gl_compiled) {glCallList(INDEXED);return triangles;}

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
	if(rr2gl_compiled && (direct || COMPILE_INDIRECT)) 
	{
		glCallList(direct?COLORED_DIRECT:COLORED_INDIRECT);
		return;
	}

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

			if(!direct && radiositySolver)
			{
				const rrVision::RRColor* indirect = radiositySolver->getTriangleRadiantExitance(0,triangleIdx,v);
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

void rr2gl_recompile()
{
	if(rr2gl_compiled) 
	{
		// next time: recompile colored indirect, others can't change
		if(COMPILE_INDIRECT)
		{
			rr2gl_compiled=false;
			glNewList(COLORED_INDIRECT,GL_COMPILE);
			rr2gl_draw_colored(false);
			glEndList();
			rr2gl_compiled=true;
		}
	}
	else
	{
		// first time: compile all
		rr2gl_compiled=false;
		glNewList(COLORED_DIRECT,GL_COMPILE);
		rr2gl_draw_colored(true);
		glEndList();
		if(COMPILE_INDIRECT)
		{
			glNewList(COLORED_INDIRECT,GL_COMPILE);
			rr2gl_draw_colored(false);
			glEndList();
		}
		glNewList(ONLYZ,GL_COMPILE);
		rr2gl_draw_onlyz();
		glEndList();
		glNewList(INDEXED,GL_COMPILE);
		rr2gl_draw_indexed();
		glEndList();
		rr2gl_compiled=true;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// rrobject load
//

void rr2gl_compile(rrVision::RRObjectImporter* aobjectImporter, rrVision::RRScene* aradiositySolver)
{
	objectImporter = aobjectImporter;
	radiositySolver = aradiositySolver;
	if(COMPILE) rr2gl_recompile();
}
