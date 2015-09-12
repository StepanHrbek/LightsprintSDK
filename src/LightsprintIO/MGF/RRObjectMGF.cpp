// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for MGF scene (testing only)
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_MGF

// This code loads MGF scene (geometry primitives, materials) into RRObjects.
//
// Unlike adapters for other formats, this one doesn't adapt 3rd party structure
// in memory, it loads scene directly form file.


#include <cmath>
#include <vector>
#include "RRObjectMGF.h"
#include "mgfparser.h"

// polygons won't share vertices if you say 0...
#define shared_vertices 1

using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// color conversion

#define  CIE_x_r	0.640f		/* nominal CRT primaries */
#define  CIE_y_r	0.330f
#define  CIE_x_g	0.290f
#define  CIE_y_g	0.600f
#define  CIE_x_b	0.150f
#define  CIE_y_b	0.060f
#define  CIE_x_w	0.3333f		/* use true white */
#define  CIE_y_w	0.3333f

#define CIE_C_rD	( (1.f/CIE_y_w) * ( CIE_x_w*(CIE_y_g - CIE_y_b) - CIE_y_w*(CIE_x_g - CIE_x_b) + CIE_x_g*CIE_y_b - CIE_x_b*CIE_y_g ) )
#define CIE_C_gD	( (1.f/CIE_y_w) * ( CIE_x_w*(CIE_y_b - CIE_y_r) - CIE_y_w*(CIE_x_b - CIE_x_r) - CIE_x_r*CIE_y_b + CIE_x_b*CIE_y_r ) )
#define CIE_C_bD	( (1.f/CIE_y_w) * ( CIE_x_w*(CIE_y_r - CIE_y_g) - CIE_y_w*(CIE_x_r - CIE_x_g) + CIE_x_r*CIE_y_g - CIE_x_g*CIE_y_r ) )


FLOAT	xyz2rgbmat[3][3] = {		/* XYZ to RGB conversion matrix */
	{(CIE_y_g - CIE_y_b - CIE_x_b*CIE_y_g + CIE_y_b*CIE_x_g)/CIE_C_rD,
	 (CIE_x_b - CIE_x_g - CIE_x_b*CIE_y_g + CIE_x_g*CIE_y_b)/CIE_C_rD,
	 (CIE_x_g*CIE_y_b - CIE_x_b*CIE_y_g)/CIE_C_rD},
	{(CIE_y_b - CIE_y_r - CIE_y_b*CIE_x_r + CIE_y_r*CIE_x_b)/CIE_C_gD,
	 (CIE_x_r - CIE_x_b - CIE_x_r*CIE_y_b + CIE_x_b*CIE_y_r)/CIE_C_gD,
	 (CIE_x_b*CIE_y_r - CIE_x_r*CIE_y_b)/CIE_C_gD},
	{(CIE_y_r - CIE_y_g - CIE_y_r*CIE_x_g + CIE_y_g*CIE_x_r)/CIE_C_bD,
	 (CIE_x_g - CIE_x_r - CIE_x_g*CIE_y_r + CIE_x_r*CIE_y_g)/CIE_C_bD,
	 (CIE_x_r*CIE_y_g - CIE_x_g*CIE_y_r)/CIE_C_bD}
};


void xy2rgb(double cx,double cy,double intensity,RRVec3& cout)
/* convert MGF color to RGB */
/* input MGF chrominance */
/* input luminance or reflectance */
/* output RGB color */
{
	static FLOAT cie[3];
	/* get CIE XYZ representation */
	cie[0] = intensity*cx/cy;
	cie[1] = intensity;
	cie[2] = intensity*(1.f/cy - 1.f) - cie[0];
	/* convert to RGB */
	cout[0] = RRReal( xyz2rgbmat[0][0]*cie[0] + xyz2rgbmat[0][1]*cie[1]	+ xyz2rgbmat[0][2]*cie[2] );
	if (cout[0] < 0.f) cout[0] = 0.f;
	cout[1] = RRReal( xyz2rgbmat[1][0]*cie[0] + xyz2rgbmat[1][1]*cie[1] + xyz2rgbmat[1][2]*cie[2] );
	if (cout[1] < 0.f) cout[1] = 0.f;
	cout[2] = RRReal( xyz2rgbmat[2][0]*cie[0] + xyz2rgbmat[2][1]*cie[1] + xyz2rgbmat[2][2]*cie[2] );
	if (cout[2] < 0.f) cout[2] = 0.f;
}

void mgf2rgb(C_COLOR *cin,FLOAT intensity,RRVec3& cout)
{
	c_ccvt(cin, C_CSXY);
	xy2rgb(cin->cx,cin->cy,intensity,cout);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectMGF : public RRObject, RRMesh
{
public:
	RRObjectMGF(const RRString& filename);
	virtual ~RRObjectMGF();

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;

	// copy of object's vertices
	struct VertexInfo
	{
		RRVec3 pos;
	};
	std::vector<VertexInfo> vertices;

	// copy of object's triangles
	struct TriangleInfo
	{
		RRMesh::Triangle indices;
	};
	std::vector<TriangleInfo> triangles;

	// copy of object's materials
	std::vector<RRMaterial*> materials;
};



//////////////////////////////////////////////////////////////////////////////
//
// callbacks for mgflib

RRObjectMGF* g_scene; // global scene necessary for callbacks

void* add_vertex(FLOAT* p,FLOAT* n)
{
	RRObjectMGF::VertexInfo v;
	v.pos = RRVec3(RRReal(p[0]),RRReal(p[1]),RRReal(p[2]));
	g_scene->vertices.push_back(v);
	return (void *)(g_scene->vertices.size()-1);
}

void* add_material(C_MATERIAL* m)
{
	RRMaterial* mat = new RRMaterial;
	mat->reset(m->sided==0);
	mgf2rgb(&m->ed_c,m->ed/4000,mat->diffuseEmittance.colorLinear); //!!!
	mgf2rgb(&m->rd_c,m->rd,mat->diffuseReflectance.colorLinear);
	mgf2rgb(&m->rs_c,m->rs,mat->specularReflectance.colorLinear);
	mgf2rgb(&m->ts_c,m->ts,mat->specularTransmittance.colorLinear);
	mat->refractionIndex = m->nr;

	// convert from physical scale, all samples expect inputs in screen colors
	RRColorSpace* colorSpace = RRColorSpace::create_sRGB();
	mat->convertFromLinear(colorSpace);
	delete colorSpace;

	g_scene->materials.push_back(mat);
	return (void *)mat;
}

void add_polygon(unsigned vertices,void** vertex,void* material)
{
	// primitive triangulation, doesn't support holes
	for (unsigned i=2;i<vertices;i++)
	{
		RRObjectMGF::TriangleInfo t;
		// this is not an error even on 64-bit platforms since MGF cannot contain over 4G of triangles
		t.indices[0] = (unsigned)((long long)vertex[0]);
		t.indices[1] = (unsigned)((long long)vertex[i-1]);
		t.indices[2] = (unsigned)((long long)vertex[i]);
		g_scene->triangles.push_back(t);
	}
	// append to facegroups
	if (g_scene->faceGroups.size() && g_scene->faceGroups[g_scene->faceGroups.size()-1].material==material)
		g_scene->faceGroups[g_scene->faceGroups.size()-1].numTriangles += vertices-2;
	else
		g_scene->faceGroups.push_back(RRObject::FaceGroup((RRMaterial*)material,vertices-2));
}

int my_hface(int ac,char** av)
{
	#define xf_xid (xf_context?xf_context->xid:0)
	if (ac<4) return(MG_EARGC);
	void **vertex=new void*[ac-1];
	for (int i = 1; i < ac; i++) {
		C_VERTEX *vp=c_getvert(av[i]);
		if (!vp) return(MG_EUNDEF);
		if (!shared_vertices || (vp->clock!=-1-111*xf_xid))
		{
			FVECT vert;
			FVECT norm;
			xf_xfmpoint(vert, vp->p);
			xf_xfmvect(norm, vp->n);
			normalize(norm);
			vp->client_data=(char *)add_vertex(vert,norm);
			vp->clock=-1-111*xf_xid;
		}
		vertex[i-1]=vp->client_data;
	}
	RR_ASSERT(c_cmaterial);
	if (c_cmaterial)
	{
		if (c_cmaterial->clock!=-1)
		{
			c_cmaterial->clock=-1;
			c_cmaterial->client_data=(char *)add_material(c_cmaterial);
		}
		add_polygon(ac-1,vertex,c_cmaterial->client_data);
	}
	delete[] vertex;
	return(MG_OK);
}

int my_hobject(int ac,char** av)
{
	//if (ac==2) begin_object(av[1]);
	//if (ac==1) end_object();
	return obj_handler(ac,av);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF load

RRObjectMGF::RRObjectMGF(const RRString& filename)
{
	// set callbacks for mgflib
	g_scene = this;
	mg_ehand[MG_E_COLOR]    = c_hcolor;	/* they get color */
	mg_ehand[MG_E_CMIX]     = c_hcolor;	/* they mix colors */
	mg_ehand[MG_E_CSPEC]    = c_hcolor;	/* they get spectra */
	mg_ehand[MG_E_CXY]      = c_hcolor;	/* they get chromaticities */
	mg_ehand[MG_E_CCT]      = c_hcolor;	/* they get color temp's */
	mg_ehand[MG_E_ED]       = c_hmaterial;	/* they get emission */
	mg_ehand[MG_E_FACE]     = my_hface;	/* we do faces */
	mg_ehand[MG_E_MATERIAL] = c_hmaterial;	/* they get materials */
	mg_ehand[MG_E_NORMAL]   = c_hvertex;	/* they get normals */
	mg_ehand[MG_E_OBJECT]   = my_hobject;	/* we track object names */
	mg_ehand[MG_E_POINT]    = c_hvertex;	/* they get points */
	mg_ehand[MG_E_RD]       = c_hmaterial;	/* they get diffuse refl. */
	mg_ehand[MG_E_RS]       = c_hmaterial;	/* they get specular refl. */
	mg_ehand[MG_E_SIDES]    = c_hmaterial;	/* they get # sides */
	mg_ehand[MG_E_TD]       = c_hmaterial;	/* they get diffuse trans. */
	mg_ehand[MG_E_TS]       = c_hmaterial;	/* they get specular trans. */
	mg_ehand[MG_E_IR]       = c_hmaterial;	/* they get refraction */
	mg_ehand[MG_E_VERTEX]   = c_hvertex;	/* they get vertices */
	mg_ehand[MG_E_POINT]    = c_hvertex;
	mg_ehand[MG_E_NORMAL]   = c_hvertex;
	mg_ehand[MG_E_XF]       = xf_handler;	/* they track transforms */
	mg_init();
	int result=mg_load(RR_RR2CHAR(filename)); // mgf does not support unicode filename. return codes are defined in mgfparser.h (success=MG_OK)
	mg_clear();
	//lu_done(&ent_tab); ent_tab is local structure inside mgflib that leaks

	// create collider
	bool aborting = false;
	setCollider(RRCollider::create(this,nullptr,RRCollider::IT_LINEAR,aborting));
}

RRObjectMGF::~RRObjectMGF()
{
	delete getCollider();
	for (unsigned i=0;i<materials.size();i++)
		delete materials[i];
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectMGF implements RRMesh

unsigned RRObjectMGF::getNumVertices() const
{
	return (unsigned)vertices.size();
}

void RRObjectMGF::getVertex(unsigned v, Vertex& out) const
{
	RR_ASSERT(v<(unsigned)vertices.size());
	out = vertices[v].pos;
}

unsigned RRObjectMGF::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObjectMGF::getTriangle(unsigned t, Triangle& out) const
{
	if (t>=RRObjectMGF::getNumTriangles()) 
	{
		RR_ASSERT(0);
		return;
	}
	out = triangles[t].indices;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectsMGF

class RRObjectsMGF : public RRObjects
{
public:
	RRObjectsMGF(const RRString& filename)
	{
		RRObjectMGF* object = new RRObjectMGF(filename);
		if (object->getNumTriangles())
			push_back(object);
		else
			delete object;
	}
	virtual ~RRObjectsMGF()
	{
		if (size())
			delete (*this)[0];
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneMGF

class RRSceneMGF : public RRScene
{
public:
	static RRScene* load(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
	{
		RRSceneMGF* scene = new RRSceneMGF;
		scene->protectedObjects = adaptObjectsFromMGF(filename);
		return scene;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFromMGF(const RRString& filename)
{
	return new RRObjectsMGF(filename);
}

void registerLoaderMGF()
{
	RRScene::registerLoader("*.mgf",RRSceneMGF::load);
}

#endif // SUPPORT_MGF