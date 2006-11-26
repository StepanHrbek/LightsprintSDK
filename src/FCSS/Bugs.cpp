#include "Bugs.h"
#include <cstdio> // printf
#include <cstdlib> // rand

using namespace rr;

extern void error(const char* message, bool gfxRelated);

class Bug
{
public:
	Bug() 
	{
		pos = RRVec3(0);
		dir = RRVec3(0);
		dist = 0;
		face = 0;
		for(unsigned i=0;i<4;i++) col[i]=rand();
	}
	void tick(float seconds, const RRScene* scene, const RRObject* object, RRRay* ray, float avgFaceArea)
	{
		//if(face!=UINT_MAX)
		{
			// get light 1 for old dir
			RRColor light1 = RRColor(0);
			RRColor light2 = RRColor(0);
			if(face!=UINT_MAX)
				scene->getTriangleMeasure(0,face,3,RM_IRRADIANCE_ALL,light1);
			// get light 2 for random new dir
			RRVec3 dir2 = RRVec3(rand()/(float)RAND_MAX,rand()/(float)RAND_MAX,rand()/(float)RAND_MAX);
			dir2 = (dir2-RRVec3(0.5f)).normalized();
			ray->rayOrigin = pos;
			ray->rayDirInv[0] = 1/dir2[0];
			ray->rayDirInv[1] = 1/dir2[1];
			ray->rayDirInv[2] = 1/dir2[2];
			ray->rayLengthMin = 0;
			ray->rayLengthMax = 1000;
			ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_DISTANCE|RRRay::TEST_SINGLESIDED;
			unsigned face2 = object->getCollider()->intersect(ray) ? ray->hitTriangle : UINT_MAX;
			if(face2!=UINT_MAX)
			{
				scene->getTriangleMeasure(0,face2,3,RM_IRRADIANCE_ALL,light2);
				float area = object->getCollider()->getMesh()->getTriangleArea(face2);
				if(area<avgFaceArea/5) face2 = UINT_MAX;
			}
			// switch to darker dir
			if(face==UINT_MAX // escaping
				|| (face2!=UINT_MAX && light2.sum()<light1.sum())) // dir2 is darker
			{
				dir = dir2;
				face = face2;
				dist = (face2==UINT_MAX) ? 1e10f : ray->hitDistance;
			}
		}
		float step = MIN(seconds,MAX(dist-0.1f,0));
		pos += dir * step;
		dist -= step;
	}
	void render()
	{
		glBegin(GL_POINTS);
		glColor3bv(col);
		glVertex3fv(&pos[0]);
		glEnd();
	}
private:
	GLbyte col[4];
	RRVec3 pos;
	RRVec3 dir; // normalized
	RRReal dist;
	unsigned face;
};

class MyBugs : public Bugs
{
public:
	MyBugs(const RRScene* ascene, const RRObject* aobject, unsigned anumBugs)
	{
		scene = ascene;
		object = aobject;
		numBugs = anumBugs;
		bugs = new Bug[numBugs];
		ray = RRRay::create();
		for(unsigned i=0;i<bugMaps;i++)
		{
			char name[]="maps\\rrbugs_bug0.tga";
			name[15] = '0'+i;
			bugMap[i] = Texture::load(name, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
			if(!bugMap[i])
			{
				printf("Texture %s not found or not supported (supported = truecolor .tga).\n",name);
				error("",false);
			}
		}
		avgFaceArea = 0;
		RRMesh* mesh = object->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();
		if(numTriangles)
		{
			for(unsigned t=0;t<numTriangles;t++) avgFaceArea += mesh->getTriangleArea(t);
			avgFaceArea /= numTriangles;
			//unsigned numDwarfs = 0;
			//for(unsigned t=0;t<numTriangles;t++) if(mesh->getTriangleArea(t)<avgFaceArea) numDwarfs++;
		}
		else
			avgFaceArea = 1;
	}

	virtual ~MyBugs()
	{
		for(unsigned i=0;i<bugMaps;i++) delete bugMap[i];
		delete[] bugs;
		delete ray;
	}

	virtual void tick(float seconds)
	{
		RRScene::setState(RRScene::GET_SOURCE,1);
		for(unsigned i=0;i<numBugs;i++)
			bugs[i].tick(seconds,scene,object,ray,avgFaceArea);
		RRScene::setState(RRScene::GET_SOURCE,0);
	}

	virtual void render()
	{
		glUseProgram(0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);
		//glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
		glTexEnvf(GL_POINT_SPRITE,GL_COORD_REPLACE,GL_TRUE);
		glEnable(GL_POINT_SPRITE);
		glPointSize(100);
		GLfloat att[3] = {0,0,1};
		glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION,att);
		glPointParameterf(GL_POINT_SPRITE_COORD_ORIGIN,GL_LOWER_LEFT);

		for(unsigned i=0;i<numBugs;i++)
		{
			bugMap[rand()%2]->bindTexture();
			bugs[i].render();
		}
		
		glPointSize(1); // nutne na ATI, jinak prepne do sw pri pristim pouziti gl_FragCoord
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}
private:
	enum {bugMaps=2};
	unsigned numBugs;
	Bug* bugs;
	const rr::RRScene* scene;
	const rr::RRObject* object;
	rr::RRRay* ray;
	Texture* bugMap[2];
	float avgFaceArea;
};

Bugs* Bugs::create(const rr::RRScene* ascene, const rr::RRObject* aobject, unsigned anumBugs)
{
	return new MyBugs(ascene,aobject,anumBugs);
}
