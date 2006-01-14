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
bool  COMPILE=0; // use display lists

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
			glVertex3fv(vertex.m);
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

	triangles=0;
	glBegin(GL_TRIANGLES);
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
			glVertex3fv(vertex.m);
		}
	}
	glEnd();
	return triangles;
}

void rr2gl_draw_colored(bool direct)
{
	if(rr2gl_compiled && direct) {glCallList(direct?COLORED_DIRECT:COLORED_INDIRECT);return;}

	glShadeModel(GL_SMOOTH);

	rrCollider::RRMeshImporter* meshImporter = objectImporter->getCollider()->getImporter();

	glBegin(GL_TRIANGLES);
	unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
	GLfloat emission[4];
	for(unsigned triangleIdx=0;triangleIdx<numTriangles;triangleIdx++)
	{
		rrCollider::RRMeshImporter::Triangle tri;
		meshImporter->getTriangle(triangleIdx,tri);
		unsigned surfaceIdx = objectImporter->getTriangleSurface(triangleIdx);
		if(surfaceIdx!=oldSurfaceIdx)
		{
			rrVision::RRSurface* surface = objectImporter->getSurface(surfaceIdx);
			assert(surface);
			if((SIDES==0 && surface->sides==1) || SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
			// hide emission from mgf
			//emission[0] = surface->diffuseEmittance*surface->diffuseEmittanceColor.m[0];
			//emission[1] = surface->diffuseEmittance*surface->diffuseEmittanceColor.m[1];
			//emission[2] = surface->diffuseEmittance*surface->diffuseEmittanceColor.m[2];
			emission[0] = 0;
			emission[1] = 0;
			emission[2] = 0;
			emission[3] = 0;
			glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,emission);
			GLfloat diffuse[4] = {surface->diffuseReflectanceColor.m[0],surface->diffuseReflectanceColor.m[1],surface->diffuseReflectanceColor.m[2],1};
			glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,diffuse);
			GLfloat specular[4] = {surface->specularReflectance,surface->specularReflectance,surface->specularReflectance,1};
			glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,specular);
		}
		if(!NORMALS) 
		{
			rrVision::RRObjectImporter::TriangleNormals triangleNormals;
			objectImporter->getTriangleNormals(triangleIdx,triangleNormals);
			glNormal3fv(triangleNormals.norm[0].m);
		}
		for(int v=0;v<3;v++) 
		{
			rrCollider::RRMeshImporter::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);

			//!!! should be uncommented, but causes problem
			// kdyz tady nenastavim emission nebo nastavim konstantu, vysledne osvetleni
			//  se nejak zmrsi, zrejme problem s kodem v register combineru
			// vyresit pouzitim opengl 2.0
			//if(!direct)
			if(radiositySolver)
			{
				const rrVision::RRColor* indirect = radiositySolver->getTriangleRadiantExitance(0,triangleIdx,v);
				GLfloat emission2[4];
				emission2[0] = emission[0] + indirect->m[0];
				emission2[1] = emission[1] + indirect->m[1];
				emission2[2] = emission[2] + indirect->m[2];
				emission2[3] = 0;
				glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,emission2);
			}

			glVertex3fv(vertex.m);
		}
	}
	glEnd();
}

void rr2gl_recompile()
{
	if(rr2gl_compiled) 
	{
		/*
		// next time: recompile colored indirect, others can't change
		rr2gl_compiled=false;
		glNewList(COLORED_INDIRECT,GL_COMPILE);
		rr2gl_draw_colored_indirect();
		glEndList();
		rr2gl_compiled=true;
		*/
	}
	else
	{
		// first time: compile all
		rr2gl_compiled=false;
		glNewList(COLORED_DIRECT,GL_COMPILE);
		rr2gl_draw_colored(true);
		glEndList();
		//glNewList(COLORED_INDIRECT,GL_COMPILE);
		//rr2gl_draw_colored(false);
		//glEndList();
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
