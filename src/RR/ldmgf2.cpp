//
// high level mgf loader by Stepan Hrbek, dee@mail.cz
//

#include <GL/glut.h>
#include <GL/glprocs.h>
#include <assert.h>
#include <math.h>
#include <memory.h>
#include "ldmgf.h"
#include "ldmgf2.h"

bool COMPILE=1; // try speedup by displaylists
int  ARRAYS =0; // try speedup by arrays, 1=small array, 2=big array
bool BUFFERS=1; // try speedup by buffers (active only when arrays>0)
int  SIDES  =0; // 1,2=force all faces 1/2-sided, 0=let them as specified by mgf
bool NORMALS=0; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)
int  MAGNIFY=10;

//////////////////////////////////////////////////////////////////////////////
//
// mgf input
//

unsigned mgf_vertices;
unsigned mgf_materials;
unsigned mgf_polygons;
unsigned mgf_polyints;
unsigned mgf_triangles;
unsigned mgf_emitors;
unsigned mgf_vertices_loaded;
unsigned mgf_materials_loaded;
unsigned mgf_polygons_loaded;
unsigned mgf_polyints_loaded;
unsigned mgf_triangles_loaded;
unsigned mgf_emitors_loaded;

GLfloat *mgf_vertex;   // point3,normal3
GLfloat *mgf_material; // sided1,emittance4,diffuse4,specular4,shininess1
int     *mgf_polyint;  // material, vertices, vertex[vertices]
int     *mgf_emitor;   // indices into polyint where emitors begin


// opengl arrays (consume additional memory for faster drawing)

#define  arr_items (3*mgf_triangles)
GLuint  *arr_vertex_indices; // small array for onlyz() (triangles * 12 bytes)
struct   Arr_item 
{
	GLfloat color[4];
	GLfloat normal[3];
	GLfloat vertex[3]; 
} *arr_interleaved; // big array for colored() (triangles * 120 bytes)


void *count_vertices(FLOAT *p,FLOAT *n)
{
	mgf_vertices++;
	return NULL;
}

void *count_materials(C_MATERIAL *material)
{
	mgf_materials++;
	return (void *)material;
}

void count_triangles(unsigned vertices,void **vertex,void *material)
{
	assert(vertices>2);
	mgf_polygons++;
	mgf_polyints+=1+1+vertices; 
	mgf_triangles+=vertices-2;
	if(((C_MATERIAL *)material)->ed) mgf_emitors++;
}

void *add_vertex(FLOAT *p,FLOAT *n)
{
	assert(mgf_vertices_loaded<mgf_vertices);
	mgf_vertex[mgf_vertices_loaded*6  ]=p[0]*MAGNIFY;
	mgf_vertex[mgf_vertices_loaded*6+1]=p[1]*MAGNIFY;
	mgf_vertex[mgf_vertices_loaded*6+2]=p[2]*MAGNIFY;
	mgf_vertex[mgf_vertices_loaded*6+3]=n[0];
	mgf_vertex[mgf_vertices_loaded*6+4]=n[1];
	mgf_vertex[mgf_vertices_loaded*6+5]=n[2];
	return (void *)(mgf_vertices_loaded++);
}

void *add_material(C_MATERIAL *m)
{
	assert(mgf_materials_loaded<mgf_materials);
	int ndx=14*mgf_materials_loaded;
	mgf_material[ndx++]=m->sided;
	RRColor col;

	mgf2rgb(&m->ed_c,m->ed,col);
	if(col.m[0]>1) col.m[0]=1;
	if(col.m[1]>1) col.m[1]=1;
	if(col.m[2]>1) col.m[2]=1;
	mgf_material[ndx++]=col[0]; // pouziva jako material emittance a light dif+ref
	mgf_material[ndx++]=col[1];
	mgf_material[ndx++]=col[2];
	mgf_material[ndx++]=m->ed?1:0;
	// pro ucely light dif+ref musi mit emitor !0, u mat.emittance je to fuk, pro ucely rozliseni co je emitor maj neemitory 0

	mgf2rgb(&m->rd_c,m->rd,col);
	mgf_material[ndx++]=col.m[0];
	mgf_material[ndx++]=col.m[1];
	mgf_material[ndx++]=col.m[2];
	mgf_material[ndx++]=1;
	mgf2rgb(&m->rs_c,m->rs,col);
	mgf_material[ndx++]=col.m[0];
	mgf_material[ndx++]=col.m[1];
	mgf_material[ndx++]=col.m[2];
	mgf_material[ndx++]=1;
	mgf_material[ndx++]=m->rs_a;
	return (void *)(mgf_materials_loaded++);
}

void add_polygon(unsigned vertices,void **vertex,void *material)
{
	int ndx=14*(int)(material);
	if(mgf_material[ndx+4]) 
	{
		assert(mgf_emitors_loaded<mgf_emitors);
		mgf_emitor[mgf_emitors_loaded++]=mgf_polyints_loaded;
	}
	assert(mgf_polygons_loaded<mgf_polygons);

	//vertexum bez normaly ji dopocita
	//predpoklada ze polygony nesdilej vertexy jinak ji dopocita pro jeden a druhej polygon ji bude mit blbe
#define MINUS(a,b,res) res[0]=a[0]-b[0];res[1]=a[1]-b[1];res[2]=a[2]-b[2]
#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define DIV(a,b,res) res[0]=a[0]/(b);res[1]=a[1]/(b);res[2]=a[2]/(b)
	GLfloat norm[3];
	GLfloat *v1=&mgf_vertex[6*(int)vertex[0]];
	GLfloat *v2=&mgf_vertex[6*(int)vertex[1]];
	GLfloat *v3=&mgf_vertex[6*(int)vertex[2]];
	GLfloat left[3];
	GLfloat right[3];
	MINUS(v1,v2,left);
	MINUS(v3,v2,right);
	CROSS(right,left,norm);
	float size=SIZE(norm);
	DIV(norm,size,norm);
	for(unsigned i=0;i<vertices;i++)
		if(mgf_vertex[6*(int)vertex[i]+3]==0 && mgf_vertex[6*(int)vertex[i]+4]==0 && mgf_vertex[6*(int)vertex[i]+5]==0)
			memcpy(mgf_vertex+6*(int)vertex[i]+3,norm,3*sizeof(GLfloat));

	mgf_polyint[mgf_polyints_loaded++]=(int)material;
	mgf_polyint[mgf_polyints_loaded++]=vertices;
	for(unsigned i=0;i<vertices;i++) mgf_polyint[mgf_polyints_loaded++]=(int)vertex[i];
	assert(mgf_polyints_loaded<=mgf_polyints);

	mgf_polygons_loaded++;
	for(unsigned i=2;i<vertices;i++)
	{
		// fill small array
		arr_vertex_indices[3*mgf_triangles_loaded+0]=(int)vertex[0];
		arr_vertex_indices[3*mgf_triangles_loaded+1]=(int)vertex[i-1];
		arr_vertex_indices[3*mgf_triangles_loaded+2]=(int)vertex[i];
		// fill big array
		memcpy(arr_interleaved[3*mgf_triangles_loaded+0].color ,&mgf_material[ndx+5]           ,4*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+1].color ,&mgf_material[ndx+5]           ,4*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+2].color ,&mgf_material[ndx+5]           ,4*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+0].normal,norm                           ,3*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+1].normal,norm                           ,3*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+2].normal,norm                           ,3*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+0].vertex,&mgf_vertex[6*(int)vertex[0]]  ,3*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+1].vertex,&mgf_vertex[6*(int)vertex[i-1]],3*sizeof(GLfloat));
		memcpy(arr_interleaved[3*mgf_triangles_loaded+2].vertex,&mgf_vertex[6*(int)vertex[i]]  ,3*sizeof(GLfloat));
		mgf_triangles_loaded++;
	}
}

// zatim nacita celou scenu do jednoho objektu, casem to predelam
//  na mnozinu objektu jak jsou deklarovany v mgf
int mgf_load(char *scenename)
{
	printf("Loading %s...\n",scenename);
	mgf_vertices=0;
	mgf_materials=0;
	mgf_polygons=0;
	mgf_polyints=0;
	mgf_triangles=0;
	mgf_emitors=0;
	mgf_vertices_loaded=0;
	mgf_materials_loaded=0;
	mgf_polygons_loaded=0;
	mgf_polyints_loaded=0;
	mgf_triangles_loaded=0;
	mgf_emitors_loaded=0;
	// zjisti kolik ceho je
	if(ldmgf(scenename,count_vertices,count_materials,count_triangles,NULL,NULL)!=MG_OK) return false;
	// naalokuje prostor
	mgf_vertex=new GLfloat[6*mgf_vertices];
	mgf_material=new GLfloat[14*mgf_materials];
	mgf_polyint=new int[mgf_polyints];
	mgf_emitor=new int[mgf_emitors];
	arr_vertex_indices=new GLuint[arr_items];
	arr_interleaved=new Arr_item[arr_items];
	// vsechno nacte
	ldmgf(scenename,add_vertex,add_material,add_polygon,NULL,NULL);

	printf("vertices: %i\n",mgf_vertices);
	printf("polygons: %i\n",mgf_polygons);
	printf("triangles: %i\n",mgf_triangles);
	printf("emitors: %i\n",mgf_emitors);
	printf("materials: %i\n",mgf_materials);
	return true;
}


//////////////////////////////////////////////////////////////////////////////
//
// mgf draw
//

#define ONLYZ   10 // displaylists
#define COLORED 11
#define COLORED_VISIBLE 12

bool mgf_compiled=false;

short ztime=1;
bool ztime_test=false;

void mgf_draw_onlyz()
{
	if(mgf_compiled) {glCallList(ONLYZ);return;}

	if(SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

	if(ARRAYS)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		// 1 -> indexed vertices (small compact array)
		if(ARRAYS==1) glDrawRangeElements(GL_TRIANGLES,0,mgf_vertices,arr_items,GL_UNSIGNED_INT,arr_vertex_indices);
		// 2 -> vertices (part of big expanded array)
		if(ARRAYS==2) glDrawArrays(GL_TRIANGLES,0,arr_items);
		glDisableClientState(GL_VERTEX_ARRAY);
		return;
	}

	unsigned i=0;
	while(i<mgf_polyints) 
	{
		i++;
		int vertices=mgf_polyint[i++] & 0xffff;
		glBegin(GL_POLYGON);
		for(int v=0;v<vertices;v++) glVertex3fv(&mgf_vertex[6*mgf_polyint[i++]]);
		glEnd();
	}
}

void mgf_draw_colored()
{
	if(mgf_compiled) {glCallList(ztime_test?COLORED_VISIBLE:COLORED);return;}

	glShadeModel(GL_SMOOTH);

	if(ARRAYS==2)
	{
		if(SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glDrawArrays(GL_TRIANGLES,0,arr_items);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		return;
	}

	if(ARRAYS)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
	}

	int curmat=-1;
	unsigned i=0;
	int p=1;
	int drawn=0;
	while(i<mgf_polyints)
	{

		if(ztime_test && (mgf_polyint[i+1]>>16)!=ztime) 
		{
			i+=2+(mgf_polyint[i+1] & 0xffff);
			p++;
		}
		else
		{
			int mat=mgf_polyint[i++];
			if(mat!=curmat)
			{
				int ndx=14*mat;
				if((mgf_material[ndx++]!=0) || SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
				glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,&mgf_material[ndx]);ndx+=4;
				glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,&mgf_material[ndx]);ndx+=4;
				glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,&mgf_material[ndx]);ndx+=4;
				//glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,mgf_material[ndx]);
				curmat=mat;
			}
			int vertices=mgf_polyint[i++] & 0xffff;

			if(ARRAYS)
			{
				assert(sizeof(GLint)==sizeof(int));
				glDrawElements(GL_POLYGON,vertices,GL_UNSIGNED_INT,mgf_polyint+i);
				i+=vertices;
			}
			else
			{
				//spocita normalu polygonu
				GLfloat norm[3];
				GLfloat *v1=&mgf_vertex[6*mgf_polyint[i+0]];
				GLfloat *v2=&mgf_vertex[6*mgf_polyint[i+1]];
				GLfloat *v3=&mgf_vertex[6*mgf_polyint[i+2]];
				GLfloat left[3];
				GLfloat right[3];
				MINUS(v1,v2,left);
				MINUS(v3,v2,right);
				CROSS(right,left,norm);
				float size=SIZE(norm);
				DIV(norm,size,norm);

				// preda vertexy
				glBegin(GL_POLYGON);
				if(!NORMALS) glNormal3fv(norm);
				for(int v=0;v<vertices;v++)
				{
					int vert=mgf_polyint[i++];
					if(NORMALS)
						if(mgf_vertex[6*vert+3] || mgf_vertex[6*vert+4] || mgf_vertex[6*vert+5]) 
							glNormal3fv(&mgf_vertex[6*vert+3]);
						else
							glNormal3fv(norm);
					glVertex3fv(&mgf_vertex[6*vert]);
				}
				glEnd();
			}

			drawn++;
		}
	}
	if(ztime_test) printf("drawn polygons: %i (%i%%)\n",drawn,drawn*100/mgf_polygons);

	if(ARRAYS)
	{
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
}

void mgf_compile()
{
	if(BUFFERS)
	{
		glBindBuffer(GL_ARRAY_BUFFER,1);
		if(ARRAYS==1) glBufferData(GL_ARRAY_BUFFER,6*sizeof(GLfloat)*mgf_vertices,mgf_vertex,GL_STATIC_DRAW);
		if(ARRAYS==2) glBufferData(GL_ARRAY_BUFFER,sizeof(Arr_item)*arr_items,arr_interleaved,GL_STATIC_DRAW);
	}
	if(ARRAYS==1)
	{
		glVertexPointer(3,GL_FLOAT,6*sizeof(GLfloat),BUFFERS?0:mgf_vertex);
		glNormalPointer(GL_FLOAT,6*sizeof(GLfloat),BUFFERS?(void*)(3*sizeof(GLfloat)):(mgf_vertex+3));
	}
	if(ARRAYS==2)
	{
		glInterleavedArrays(GL_C4F_N3F_V3F,0,BUFFERS?0:arr_interleaved);
	}
	if(COMPILE)
	{
		glNewList(COLORED,GL_COMPILE);
		mgf_draw_colored();
		glEndList();
		glNewList(ONLYZ,GL_COMPILE);
		mgf_draw_onlyz();
		glEndList();
		mgf_compiled=true;
	}
}
