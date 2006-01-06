//
// OpenGL renderer of RRObjectImporter by Stepan Hrbek, dee@mail.cz
//

#include <GL/glut.h>
#include <GL/glprocs.h>
#include <assert.h>
#include <math.h>
#include "rr2gl.h"


int   SIDES  =2; // 1,2=force all faces 1/2-sided, 0=let them as specified by mgf
bool  NORMALS=0; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)

//////////////////////////////////////////////////////////////////////////////
//
// rrobject input
//

rrVision::RRObjectImporter* objectImporter = NULL;


//////////////////////////////////////////////////////////////////////////////
//
// rrobject draw
//

#define ONLYZ   10 // displaylists
#define COLORED 11
#define INDEXED 12

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

void rr2gl_draw_colored()
{
	if(rr2gl_compiled) {glCallList(COLORED);return;}

	glShadeModel(GL_SMOOTH);

	rrCollider::RRMeshImporter* meshImporter = objectImporter->getCollider()->getImporter();

	glBegin(GL_TRIANGLES);
	unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
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
			GLfloat emission[4] = {surface->diffuseEmittance*surface->diffuseEmittanceColor.m[0],surface->diffuseEmittance*surface->diffuseEmittanceColor.m[1],surface->diffuseEmittance*surface->diffuseEmittanceColor.m[2],1};
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

void rr2gl_compile()
{
	glNewList(COLORED,GL_COMPILE);
	rr2gl_draw_colored();
	glEndList();
	glNewList(ONLYZ,GL_COMPILE);
	rr2gl_draw_onlyz();
	glEndList();
	glNewList(INDEXED,GL_COMPILE);
	rr2gl_draw_indexed();
	glEndList();
	rr2gl_compiled=true;
}


//////////////////////////////////////////////////////////////////////////////
//
// rrobject load
//

void rr2gl_load(rrVision::RRObjectImporter* aobjectImporter)
{
	objectImporter = aobjectImporter;
	rr2gl_compile();
}
