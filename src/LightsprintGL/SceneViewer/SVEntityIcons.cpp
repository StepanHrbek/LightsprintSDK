// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVEntityIcons.h"

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/Timer.h"
#include "../PreserveState.h"
#include "../tmpstr.h"
#include "SVEntity.h"

namespace rr_gl
{

SVEntityIcons::SVEntityIcons(const char* pathToMaps, UberProgram* uberProgram)
{
	icon[rr::RRLight::DIRECTIONAL] = rr::RRBuffer::load(tmpstr("%ssv_sun.png",pathToMaps));
	icon[rr::RRLight::POINT] = rr::RRBuffer::load(tmpstr("%ssv_point.png",pathToMaps));
	icon[rr::RRLight::SPOT] = rr::RRBuffer::load(tmpstr("%ssv_spot.png",pathToMaps));

	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
	uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = true;
	program = uberProgramSetup.getProgram(uberProgram);
}

SVEntityIcons::~SVEntityIcons()
{
	delete icon[rr::RRLight::DIRECTIONAL];
	delete icon[rr::RRLight::POINT];
	delete icon[rr::RRLight::SPOT];
}

// inputs: ray->rayXxx
// outputs: ray->hitXxx, hitTriangle=entity index in entities
// sideeffects: ray->rayLengthMax is lost
bool SVEntityIcons::intersectIcons(const SVEntities& entities, rr::RRRay* ray, float iconSize)
{
	RR_ASSERT(ray);
	bool hit = false;
	unsigned counter = 0;
	for (unsigned i=0;i<entities.size();i++)
	{
		if (intersectIcon(entities[i],ray,iconSize))
		{
			// we have a hit, stop searching in greater distance
			hit = true;
			ray->rayLengthMax = ray->hitDistance;
			// return entity index in hitTriangle
			ray->hitTriangle = i;
		}
	}
	return hit;
}

void SVEntityIcons::renderIcons(const SVEntities& entities, const Camera& eye, unsigned selectedIndex, float iconSize)
{
	// setup for rendering icon
	PreserveBlend p1;
	PreserveBlendFunc p2;
	PreserveAlphaTest p3;
	PreserveAlphaFunc p4;

	// keyed icons (alpha0=transparent)
	glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.03f);
	// blended icons (alpha0=transparent)
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// blended icons (white=transparent)
	//glEnable(GL_BLEND); glBlendFunc(GL_ONE_MINUS_SRC_COLOR,GL_SRC_COLOR);

	program->useIt();
	program->sendUniform("lightIndirectConst",1.0f,1.0f,1.0f,1.0f);
	glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_DIFFUSE);
	program->sendUniform("materialDiffuseMap", TEXTURE_2D_MATERIAL_DIFFUSE);

	// render icons
	unsigned counter = 0;
	for (unsigned i=0;i<entities.size();i++)
	{
		if (i==0 || i==selectedIndex+1)
		{
			program->sendUniform("lightIndirectConst",1.0f,1.0f,1.0f,1.0f);
		}
		if (i==selectedIndex)
		{
			float time = fabs(fmod((float)(GETSEC),1.0f));
			program->sendUniform("lightIndirectConst",1.0f+time,1.0f+time,1.0f+time,1.0f);
		}
		renderIcon(entities[i],eye,iconSize);
	}
}

// icon vertices are computed in worldspace to simplify ray-icon intersections
//kdybych kreslil 3d objekt, prusecik je snadny, udelam z nej RRObject, jen mu zmenim matici
void SVEntityIcons::getIconWorldVertices(const SVEntity& entity, rr::RRVec3 eyePos, rr::RRVec3 vertex[4], float iconSize)
{
//!!! kdyz je zarovka nad kamerou, kresli ji strasne malinkou
	rr::RRVec3 toLight = (entity.position-eyePos).normalizedSafe();
	rr::RRVec3 toLeftFromLight(toLight[2],0,-toLight[0]);
	rr::RRVec3 toUpFromLight = toLight.cross(toLeftFromLight);
	float lightDistance = (entity.position-eyePos).length();
	vertex[0] = entity.position-(toLeftFromLight-toUpFromLight)*iconSize; // top left
	vertex[1] = entity.position+(toLeftFromLight+toUpFromLight)*iconSize; // top right
	vertex[2] = entity.position+(toLeftFromLight-toUpFromLight)*iconSize; // bottom right
	vertex[3] = entity.position-(toLeftFromLight+toUpFromLight)*iconSize; // bottom left
}

// inputs: ray->rayXxx
// outputs: ray->hitXxx
bool SVEntityIcons::intersectTriangle(const rr::RRMesh::TriangleBody* t, rr::RRRay* ray)
{
	RR_ASSERT(ray);
	RR_ASSERT(t);

	rr::RRVec3 rayDir = rr::RRVec3(1)/ray->rayDirInv;

	// calculate determinant - also used to calculate U parameter
	rr::RRVec3 pvec = rayDir.cross(t->side2);
	float det = t->side1.dot(pvec);

	// cull test
	bool hitFrontSide = det>0;
	if (!hitFrontSide && (ray->rayFlags&rr::RRRay::TEST_SINGLESIDED)) return false;

	// if determinant is near zero, ray lies in plane of triangle
	if (det==0) return false;

	// calculate distance from vert0 to ray origin
	rr::RRVec3 tvec = ray->rayOrigin-t->vertex0;

	// calculate U parameter and test bounds
	float u = tvec.dot(pvec)/det;
	if (u<0 || u>1) return false;

	// prepare to test V parameter
	rr::RRVec3 qvec = tvec.cross(t->side1);

	// calculate V parameter and test bounds
	float v = rayDir.dot(qvec)/det;
	if (v<0 || u+v>1) return false;

	// calculate distance where ray intersects triangle
	float dist = t->side2.dot(qvec)/det;
	if (dist<ray->rayLengthMin || dist>ray->rayLengthMax) return false;

	ray->hitDistance = dist;
	return true;
}

// inputs: ray->rayXxx
// outputs: ray->hitXxx
bool SVEntityIcons::intersectIcon(const SVEntity& entity, rr::RRRay* ray, float iconSize)
{
	RR_ASSERT(ray);
	rr::RRVec3 worldVertex[4];
	getIconWorldVertices(entity,ray->rayOrigin,worldVertex,iconSize);
	rr::RRMesh::TriangleBody tb1,tb2;
	tb1.vertex0 = worldVertex[0];
	tb1.side1 = worldVertex[1]-worldVertex[0];
	tb1.side2 = worldVertex[2]-worldVertex[0];
	tb2.vertex0 = worldVertex[0];
	tb2.side1 = worldVertex[2]-worldVertex[0];
	tb2.side2 = worldVertex[3]-worldVertex[0];
	bool hit = intersectTriangle(&tb1,ray) || intersectTriangle(&tb2,ray);
	return hit;
}

void SVEntityIcons::renderIcon(const SVEntity& entity, const Camera& eye, float iconSize)
{
	if (icon[entity.icon])
	{
		rr::RRVec3 worldVertex[4];
		getIconWorldVertices(entity,eye.pos,worldVertex,iconSize);

		getTexture(icon[entity.icon])->bindTexture();

		glBegin(GL_QUADS);
		glTexCoord2f(1,1);
		glVertex3fv(&worldVertex[0][0]);
		glTexCoord2f(0,1);
		glVertex3fv(&worldVertex[1][0]);
		glTexCoord2f(0,0);
		glVertex3fv(&worldVertex[2][0]);
		glTexCoord2f(1,0);
		glVertex3fv(&worldVertex[3][0]);
		glEnd();
	}
}

bool SVEntityIcons::isOk() const
{
	return icon[0] && icon[1] && icon[2];
}

}; // namespace

#endif // SUPPORT_SCENEVIEWER
