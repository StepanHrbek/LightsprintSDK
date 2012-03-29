// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVEntityIcons.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "../PreserveState.h"
#include "wx/wx.h"

#define ICONS_ON_TOP // icons not occluded by geometry

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// SVEntities - all entities in SceneViewer

void SVEntities::addLights(const rr::RRLights& lights, rr::RRVec3 dirlightPosition)
{
	for (unsigned i=0;i<lights.size();i++)
	{
		if (lights[i] && lights[i]->name!="Flashlight")
		{
			SVEntity entity;
			entity.type = ST_LIGHT;
			entity.index = i;
			switch (lights[i]->type)
			{
				case rr::RRLight::DIRECTIONAL:
					entity.iconCode = IC_DIRECTIONAL;
					entity.position = dirlightPosition;
					dirlightPosition.y += 1;
					break;
				case rr::RRLight::POINT:
					entity.iconCode = IC_POINT;
					entity.position = lights[i]->position;
					break;
				case rr::RRLight::SPOT:
					entity.iconCode = IC_SPOT;
					entity.position = lights[i]->position;
					break;
				default:
					RR_ASSERT(0);
			}
			entity.iconSize = iconSize;
			entity.bright = lights[i]->enabled;
			entity.parentPosition = entity.position;
			push_back(entity);
		}
	}
}


void SVEntities::markSelected(const EntityIds& selectedEntityIds)
{
	for (unsigned i=0;i<size();i++)
		(*this)[i].selected = selectedEntityIds.find((*this)[i])!=selectedEntityIds.end();
}

void SVEntities::addXYZ(rr::RRVec3 center, IconCode transformation)
{
	float dist = 2*iconSize*5.5f;
	SVEntity e;
	e.position = center;
	e.parentPosition = center;
	e.iconSize = 2*iconSize;
	e.iconCode = transformation;
	e.bright = true;
	e.selected = false;
	push_back(e);
	e.iconSize *= 0.6f;
	e.position.x += dist;
	e.iconCode = IC_X;
	push_back(e);
	e.position.x -= dist;
	e.position.y += dist;
	e.iconCode = IC_Y;
	push_back(e);
	e.position.y -= dist;
	e.position.z += dist;
	e.iconCode = IC_Z;
	push_back(e);
}


/////////////////////////////////////////////////////////////////////////////
//
// SVEntityIcons

SVEntityIcons::SVEntityIcons(const char* pathToMaps, UberProgram* uberProgram)
{
	const char* filename[] = {"sv_point","sv_spot","sv_sun","sv_movement","sv_rotation","sv_scale","sv_static","sv_x","sv_y","sv_z"};
	for (IconCode i=IC_POINT;i!=IC_LAST;i=(IconCode)(i+1))
		icon[i] = rr::RRBuffer::load(RR_WX2RR(wxString::Format("%s%s.png",pathToMaps,filename[i])));

	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	programArrows = uberProgramSetup.getProgram(uberProgram);
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
	uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = true;
	programIcons = uberProgramSetup.getProgram(uberProgram);
}

SVEntityIcons::~SVEntityIcons()
{
	for (unsigned i=0;i<IC_LAST;i++)
		delete icon[i];
}

// inputs: ray->rayXxx
// outputs: ray->hitXxx, hitTriangle=entity index in entities
// sideeffects: ray->rayLengthMax is lost
bool SVEntityIcons::intersectIcons(const SVEntities& entities, rr::RRRay* ray)
{
	RR_ASSERT(ray);
	bool hit = false;
	unsigned counter = 0;
	for (unsigned i=0;i<entities.size();i++)
	{
		if (intersectIcon(entities[i],ray))
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

void SVEntityIcons::renderIcons(const SVEntities& entities, const rr::RRCamera& eye)
{
	// setup for rendering icon
	PreserveBlend p1;
	PreserveBlendFunc p2;
	PreserveAlphaTest p3;
	PreserveAlphaFunc p4;
#ifdef ICONS_ON_TOP
	PreserveDepthTest p5;
#endif

	// keyed icons (alpha0=transparent)
	glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.03f);
	// blended icons (alpha0=transparent)
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// blended icons (white=transparent)
	//glEnable(GL_BLEND); glBlendFunc(GL_ONE_MINUS_SRC_COLOR,GL_SRC_COLOR);
#ifdef ICONS_ON_TOP
	glDisable(GL_DEPTH_TEST);
#endif

	// render icons
	if (programIcons)
	{
		programIcons->useIt();
		programIcons->sendUniform("lightIndirectConst",rr::RRVec4(1));
		programIcons->sendTexture("materialDiffuseMap", NULL); // renderIcon() will repeatedly bind texture
		unsigned counter = 0;
		for (unsigned i=0;i<entities.size();i++)
		{
			static rr::RRTime time;
			float brightness = entities[i].selected ? 1+fabs(fmod((float)(time.secondsPassed()),1.0f)) : (entities[i].bright?1:0.3f);
			programIcons->sendUniform("lightIndirectConst",rr::RRVec4(brightness,brightness,brightness,1.0f));
			renderIcon(entities[i],eye);
		}
	}

	// render arrows
	programArrows->useIt();
	glLineWidth(5);
	for (unsigned i=0;i<entities.size();i++)
	{
		renderArrow(entities[i],eye);
	}
	glLineWidth(1);
}

// icon vertices are computed in worldspace to simplify ray-icon intersections
//kdybych kreslil 3d objekt, prusecik je snadny, udelam z nej RRObject, jen mu zmenim matici
void SVEntityIcons::getIconWorldVertices(const SVEntity& entity, rr::RRVec3 eyePos, rr::RRVec3 vertex[4])
{
	rr::RRVec3 toLight = (entity.position-eyePos).normalizedSafe();
	rr::RRVec3 toLeftFromLight = rr::RRVec3(toLight[2],0,-toLight[0]).normalizedSafe();
	rr::RRVec3 toUpFromLight = toLight.cross(toLeftFromLight);
	float lightDistance = (entity.position-eyePos).length();
	vertex[0] = entity.position-(toLeftFromLight-toUpFromLight)*entity.iconSize; // top left
	vertex[1] = entity.position+(toLeftFromLight+toUpFromLight)*entity.iconSize; // top right
	vertex[2] = entity.position+(toLeftFromLight-toUpFromLight)*entity.iconSize; // bottom right
	vertex[3] = entity.position-(toLeftFromLight+toUpFromLight)*entity.iconSize; // bottom left
}

// inputs: ray->rayXxx
// outputs: ray->hitXxx
bool SVEntityIcons::intersectTriangle(const rr::RRMesh::TriangleBody* t, rr::RRRay* ray)
{
	RR_ASSERT(ray);
	RR_ASSERT(t);

	// calculate determinant - also used to calculate U parameter
	rr::RRVec3 pvec = ray->rayDir.cross(t->side2);
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
	float v = ray->rayDir.dot(qvec)/det;
	if (v<0 || u+v>1) return false;

	// calculate distance where ray intersects triangle
	float dist = t->side2.dot(qvec)/det;
#ifdef ICONS_ON_TOP
	if (dist<ray->rayLengthMin || (dist>ray->rayLengthMax && ray->hitTriangle==UINT_MAX)) return false; // ignore rayLengthMax if hitTriangle is set, RL has icons on top of meshes
	ray->hitTriangle = UINT_MAX; // clear hitTriangle, from now on icon distance will be compared properly
#else
	if (dist<ray->rayLengthMin || dist>ray->rayLengthMax) return false;
#endif
	ray->hitDistance = dist;
	return true;
}

// inputs: ray->rayXxx
// outputs: ray->hitXxx
bool SVEntityIcons::intersectIcon(const SVEntity& entity, rr::RRRay* ray)
{
	RR_ASSERT(ray);
	rr::RRVec3 worldVertex[4];
	getIconWorldVertices(entity,ray->rayOrigin,worldVertex);
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

void SVEntityIcons::renderIcon(const SVEntity& entity, const rr::RRCamera& eye)
{
	if (entity.iconCode>=0 && entity.iconCode<IC_LAST && icon[entity.iconCode])
	{
		rr::RRVec3 worldVertex[4];
		getIconWorldVertices(entity,eye.getPosition(),worldVertex);

		getTexture(icon[entity.iconCode])->bindTexture();

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

void SVEntityIcons::renderArrow(const SVEntity& entity, const rr::RRCamera& eye)
{
	if (entity.parentPosition!=entity.position)
	{
		rr::RRVec3 a(entity.parentPosition);
		rr::RRVec3 b(entity.position);
		b = a*0.15f+b*0.85f; // make arrow little bit shorter
		rr::RRVec3 c = eye.getDirection().cross(b-a)*0.1f;
		rr::RRVec3 tmp1(a*0.1f+b*0.9f+c);
		rr::RRVec3 tmp2(a*0.1f+b*0.9f-c);
		programArrows->sendUniform("lightIndirectConst",rr::RRVec4((b-a).abs().normalized(),1.0f));
		glBegin(GL_LINES);
		glVertex3fv(&b.x);
		glVertex3fv(&a.x);
		glVertex3fv(&b.x);
		glVertex3fv(&tmp1.x);
		glVertex3fv(&b.x);
		glVertex3fv(&tmp2.x);
		glEnd();
	}
}

bool SVEntityIcons::isOk() const
{
	for (unsigned i=0;i<IC_LAST;i++)
		if (!icon[i]) return false;
	return true;
}

}; // namespace

#endif // SUPPORT_SCENEVIEWER
