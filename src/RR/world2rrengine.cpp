#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ldmgf.h"
#include "RREngine.h"
#include "world2rrengine.h"
#include "world2rrintersect.h"
#include "surface.h"

#define DBG(a) a //!!!

//////////////////////////////////////////////////////////////////////////////
//
// WorldSceneImporter

class WorldObjectImporter : public rrEngine::RRObjectImporter
{
public:
	WorldObjectImporter(WORLD* aworld, OBJECT* aobject, Surface** asurface, unsigned asurfaces);
	virtual ~WorldObjectImporter();

	// must not change during object lifetime
	virtual const rrIntersect::RRCollider* getCollider() const {return collider;}
	virtual unsigned     getTriangleSurface(unsigned t) const;
	virtual RRSurface*   getSurface(unsigned si);

	// may change during object lifetime
	virtual const float* getWorldMatrix();
	virtual const float* getInvWorldMatrix();

private:
	WORLD*      world;
	OBJECT*     object;
	Surface**   surface;
	unsigned    surfaces;
	rrIntersect::RRCollider* collider;
};

WorldObjectImporter::WorldObjectImporter(WORLD* aworld, OBJECT* aobject, Surface** asurface, unsigned asurfaces)
{
	world = aworld;
	object = aobject;
	surface = asurface;
	surfaces = asurfaces;
	collider = rrIntersect::RRCollider::create(new WorldMeshImporter(aobject),(rrIntersect::RRCollider::IntersectTechnique)RRGetState(RRSS_INTERSECT_TECHNIQUE));
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
	assert(i<object->face_num);
	return object->face[i].material;
}

RRSurface* WorldObjectImporter::getSurface(unsigned si)
{
	assert(si<surfaces);
	if(si>=surfaces) return surface[0];
	return surface[si];
}

const float* WorldObjectImporter::getWorldMatrix()
{
	return object->matrix[0];
}

const float* WorldObjectImporter::getInvWorldMatrix()
{
	return object->inverse[0];
}

//////////////////////////////////////////////////////////////////////////////
//
// mgf material loader
//

static void fillColorTable(unsigned *ct,double cx,double cy,real rs)
{
	for(unsigned c=0;c<C_INDICES;c++)
	{
		real rgb[3];
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
	s->sides                =(m->sided==1)?1:2;
	s->diffuseReflectance   =m->rd;
	xy2rgb(m->rd_c.cx,m->rd_c.cy,0.5,s->diffuseReflectanceColor);
	s->diffuseReflectanceColor[0]*=m->rd;
	s->diffuseReflectanceColor[1]*=m->rd;
	s->diffuseReflectanceColor[2]*=m->rd;
	s->diffuseReflectanceColorTable=createColorTable(m->rd_c.cx,m->rd_c.cy,m->rs);
	s->diffuseTransmittance =m->td;
	xy2rgb(m->td_c.cx,m->td_c.cy,0.5,s->diffuseTransmittanceColor);
	s->diffuseEmittance     =m->ed/1000;
	xy2rgb(m->ed_c.cx,m->ed_c.cy,0.5,s->diffuseEmittanceColor);
	s->emittanceType        =diffuseLight;
	//s->emittancePoint       =Point3(0,0,0);
	s->specularReflectance  =m->rs;
	s->specularTransmittance=m->ts;
	s->refractionReal       =m->nr;
	s->refractionImaginary  =m->ni;
	s->outside              =NULL;
	s->inside               =NULL;
	s->texture              =NULL;
	s->_rd                  =m->rd;
	s->_rdcx                =m->rd_c.cx;
	s->_rdcy                =m->rd_c.cy;
	s->_ed                  =m->ed/1000;
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
	s_default.sides=2; // 1 if surface is 1-sided, 2 for 2-sided
	s_default.diffuseReflectance=0.3;
	s_default.diffuseReflectanceColor[0]=1;
	s_default.diffuseReflectanceColor[1]=1;
	s_default.diffuseReflectanceColor[2]=1;
	s_default.diffuseReflectanceColorTable=createColorTable(0.3,0.3,0);
	s_default.diffuseTransmittance=0;
	//s_default.diffuseTransmittanceColor={1,1,1};
	s_default.diffuseEmittance=1;
	s_default.diffuseEmittanceColor[0]=1;
	s_default.diffuseEmittanceColor[1]=1;
	s_default.diffuseEmittanceColor[2]=1;
	s_default.emittanceType=diffuseLight;
	//s_default.emittancePoint=Vec3(0,0,0);
	s_default.specularReflectance=0;
	s_default.specularReflectanceColor[0]=1;
	s_default.specularReflectanceColor[1]=1;
	s_default.specularReflectanceColor[2]=1;
	s_default.specularReflectanceRoughness=0.5;
	s_default.specularTransmittance=0;
	//s_default.specularTransmittanceColor={1,1,1};
	s_default.specularTransmittanceRoughness=0.5;
	s_default.refractionReal=1;
	s_default.refractionImaginary=0;
	s_default.outside=NULL;
	s_default.inside=NULL;
	s_default.texture=NULL;
	s_default._rd=0.3;//needed when calculating different illumination for different components
	s_default._rdcx=0.3;
	s_default._rdcy=0.3;
	s_default._ed=0;//needed by turnLight
	
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

RRScene *convert_world2scene(WORLD *world, char *material_mgf)
{
	// load surfaces
	load_materials(world, material_mgf);
	// load geometry
	RRScene *rrscene=new RRScene();
	DBG(printf("Loading geometry...\n"));
	for (int o=0;o<world->object_num;o++) 
	{
		// dynamic = w->object[o].pos.num!=1 || w->object[o].rot.num!=1
		WorldObjectImporter* importer = new WorldObjectImporter(world, &world->object[o], scene_surface_ptr, scene_surfaces);
		RRScene::ObjectHandle handle = rrscene->objectCreate(importer);
		world->object[o].obj = rrscene->getObject(handle);
	}	
	return rrscene;
}
