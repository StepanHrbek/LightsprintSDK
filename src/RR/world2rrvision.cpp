#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ldmgf.h"
#include "render.h"
#include "RRVision.h"
#include "surface.h"
#include "world2rrvision.h"
#include "world2rrintersect.h"

#define DBG(a) a //!!!

//////////////////////////////////////////////////////////////////////////////
//
// WorldSceneImporter

class WorldObjectImporter : public rrVision::RRObjectImporter
{
public:
	WorldObjectImporter(WORLD* aworld, OBJECT* aobject, Surface** asurface, unsigned asurfaces, rrCollider::RRCollider::IntersectTechnique intersectTechnique);
	virtual ~WorldObjectImporter();

	// must not change during object lifetime
	virtual const rrCollider::RRCollider* getCollider() const {return collider;}
	virtual unsigned           getTriangleSurface(unsigned t) const;
	virtual const RRSurface*   getSurface(unsigned si) const;

	// may change during object lifetime
	virtual const RRMatrix3x4* getWorldMatrix();
	virtual const RRMatrix3x4* getInvWorldMatrix();

private:
	WORLD*      world;
	OBJECT*     object;
	Surface**   surface;
	unsigned    surfaces;
	rrCollider::RRCollider* collider;
	RRMatrix3x4 matrix;
	RRMatrix3x4 inverse;
};

WorldObjectImporter::WorldObjectImporter(WORLD* aworld, OBJECT* aobject, Surface** asurface, unsigned asurfaces, rrCollider::RRCollider::IntersectTechnique intersectTechnique)
{
	world = aworld;
	object = aobject;
	surface = asurface;
	surfaces = asurfaces;
	collider = rrCollider::RRCollider::create(new WorldMeshImporter(aobject),intersectTechnique);
	assert(world);
	assert(surface);

	// scale pokus
	//for(int i=0;i<3;i++)for(int j=0;j<3;j++) object->matrix[i][j]/=4;
	//matrix_Invert(object->matrix,object->inverse);
}

WorldObjectImporter::~WorldObjectImporter()
{
}

unsigned WorldObjectImporter::getTriangleSurface(unsigned i) const
{
	assert(object);
	assert(i<(unsigned)object->face_num);
	return object->face[i].material;
}

const RRSurface* WorldObjectImporter::getSurface(unsigned si) const
{
	assert(si<surfaces);
	if(si>=surfaces) return surface[0];
	return surface[si];
}

const RRMatrix3x4* WorldObjectImporter::getWorldMatrix()
{
	// from WORLD's column-major to Lightsprint's row-major
	for(unsigned j=0;j<3;j++)
		for(unsigned i=0;i<4;i++)
			matrix.m[j][i] = object->matrix[i][j];
	return &matrix;
}

const RRMatrix3x4* WorldObjectImporter::getInvWorldMatrix()
{
	// from WORLD's column-major to Lightsprint's row-major
	for(unsigned j=0;j<3;j++)
		for(unsigned i=0;i<4;i++)
			inverse.m[j][i] = object->inverse[i][j];
	return &inverse;
}

//////////////////////////////////////////////////////////////////////////////
//
// mgf material loader
//

static void fillColorTable(unsigned *ct,double cx,double cy,real rs)
{
	for(unsigned c=0;c<C_INDICES;c++)
	{
		RRColor rgb;
		xy2rgb(cx,cy,c/255.,rgb);
#define FLOAT2BYTE(f) ((unsigned)((f)<0?0:(f)>=1?255:256*(f)))
		ct[c]=(FLOAT2BYTE(rs)<<24) + (FLOAT2BYTE(rgb[0])<<16) + (FLOAT2BYTE(rgb[1])<<8) + FLOAT2BYTE(rgb[2]);
	}
}

static ColorTable createColorTable(double cx,double cy,real rs)
{
	ColorTable ct=new unsigned[C_INDICES];
	fillColorTable(ct,cx,cy,rs);
	return ct;
}

static void fillSurface(Surface *s,C_MATERIAL *m)
{
	s->reset(m->sided==2 || m->ts); // pruhledne by melo reagovat z obou stran, pokud to klient nechce tak mu to vnutime
	xy2rgb(m->rd_c.cx,m->rd_c.cy,0.5,s->diffuseReflectance);
	s->diffuseReflectance        *=m->rd;
	s->diffuseReflectanceColorTable=createColorTable(m->rd_c.cx,m->rd_c.cy,m->rs);
	xy2rgb(m->ed_c.cx,m->ed_c.cy,0.5,s->diffuseEmittance);
	s->diffuseEmittance          *=m->ed/1000;
	s->specularReflectance        =m->rs;
	s->specularTransmittance      =m->ts;
	s->refractionReal             =m->nr;
	s->outside                    =NULL;
	s->inside                     =NULL;
	s->texture                    =NULL;
}

unsigned    scene_surfaces;
unsigned    scene_surfaces_loaded;
Surface*    scene_surface;
Surface**   scene_surface_ptr;

static void *scene_add_surface(C_MATERIAL *m)
{
#ifdef TEST_SCENE
	if(scene_surfaces_loaded<0 || scene_surfaces_loaded>=scene_surfaces) {scene_surfaces_loaded++;return NULL;}
#endif
	Surface *s=&scene_surface[scene_surfaces_loaded++];
	fillSurface(s,m);
	//printf("Filled surface[%d] rs=%f\n",mgf_surfaces_loaded-1,s->specularReflectance);
	return s;
}

static void load_materials(WORLD* world, char *material_mgf)
{
	DBG(printf("Loading surfaces...\n"));
	scene_surfaces=world->material_num;
	scene_surface=new Surface[scene_surfaces]; //!!! won't be freed
	scene_surface_ptr=new Surface*[scene_surfaces]; //!!! won't be freed
	scene_surfaces_loaded=0;
	ldmgf(material_mgf,NULL,scene_add_surface,NULL,NULL,NULL);// nacte knihovnu materialu	(stejne_jmeno.mgf)

	static Surface s_default;
	s_default.reset(true);
	s_default.diffuseReflectance[0]=0.3;
	s_default.diffuseReflectance[1]=0.3;
	s_default.diffuseReflectance[2]=0.3;
	s_default.diffuseReflectanceColorTable=createColorTable(0.3,0.3,0);
	s_default.diffuseEmittance=RRColor(1);
	s_default.outside=NULL;
	s_default.inside=NULL;
	s_default.texture=NULL;
	
	for(int si=0;si<world->material_num;si++)
	{
		Surface* s = NULL;
		/*if(  (__hidematerial1 && expmatch(w->material[si].name,__hidematerial1)) 
			|| (__hidematerial2 && expmatch(w->material[si].name,__hidematerial2))
			|| (__hidematerial3 && expmatch(w->material[si].name,__hidematerial3)))
		{
			s=NULL;
		}
		else*/
		{
			C_MATERIAL *m=c_getmaterial(world->material[si].name);
#ifdef TEST_SCENE
			if(!m) 
			{
				printf("# Unknown material %s.\n",world->material[si].name);
				//s=&s_default;
				s=&scene_surface[si];
			} else 
#endif
			{
				s=&scene_surface[si];
				fillSurface(s,m);
			}
		}
		scene_surface_ptr[si] = s;
	}
}

RRScene *convert_world2scene(WORLD *world, char *material_mgf, rrCollider::RRCollider::IntersectTechnique intersectTechnique)
{
	// load surfaces
	load_materials(world, material_mgf);
	// load geometry
	RRScene *rrscene=new RRScene();
	DBG(printf("Loading geometry...\n"));
	for(int o=0;o<world->object_num;o++) 
	{
		// dynamic = w->object[o].pos.num!=1 || w->object[o].rot.num!=1
		WorldObjectImporter* importer = new WorldObjectImporter(world, &world->object[o], scene_surface_ptr, scene_surfaces, intersectTechnique);
		world->object[o].objectHandle = rrscene->objectCreate(importer);
	}	
	//rrscene->sceneFreeze(true); //speedup for multiobject scenes, not necessary for rr
	return rrscene;
}
