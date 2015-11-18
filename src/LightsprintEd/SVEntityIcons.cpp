// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVEntityIcons.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/PreserveState.h"
#include "wx/wx.h"

namespace rr_ed
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
					dirlightPosition.y += 4*iconSize; // the same constant is in getCenterOf()
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

void SVEntities::addXYZ(rr::RRVec3 center, IconCode transformation, const rr::RRCamera& eye)
{
	float size1 = (eye.getPositionInViewport(center+eye.getRight()*iconSize)-eye.getPositionInViewport(center)).RRVec2::length()*2;
	float size2 = RR_CLAMPED(size1,0.02f,0.2f);
	float adjustedIconSize = iconSize*size2/size1;

	float dist = adjustedIconSize*5.5f;
	SVEntity e;
	e.position = center;
	e.parentPosition = center;
	e.iconSize = adjustedIconSize;
	e.bright = true;
	e.selected = false;
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
	e.position.z -= dist;
	e.iconCode = transformation;
	push_back(e);
}


/////////////////////////////////////////////////////////////////////////////
//
// SVEntityIcons

SVEntityIcons::SVEntityIcons(wxString pathToMaps, rr_gl::UberProgram* uberProgram)
{
	const char* filename[] = {"sv_point","sv_spot","sv_sun","sv_movement","sv_rotation","sv_scale","sv_static","sv_x","sv_y","sv_z"};
	for (IconCode i=IC_POINT;i!=IC_LAST;i=(IconCode)(i+1))
		icon[i] = rr::RRBuffer::load(RR_WX2RR(pathToMaps+filename[i]+".png"));

	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.LEGACY_GL = 1;
	programArrows = uberProgramSetup.getProgram(uberProgram);
}

bool SVEntityIcons::isOk() const
{
	for (unsigned i=0;i<IC_LAST;i++)
		if (!icon[i]) return false;
	return true;
}

void SVEntityIcons::renderIcons(const SVEntities& entities, rr_gl::TextureRenderer* textureRenderer, const rr::RRCamera& eye, const rr::RRCollider* supercollider, rr::RRRay* ray, const SceneViewerStateEx& svs)
{
	if (!programArrows)
		return;
	// render arrows
	rr_gl::PreserveFlag p0(GL_DEPTH_TEST,false);
	programArrows->useIt();
	uberProgramSetup.useCamera(programArrows,&eye);
	glLineWidth(5);
	for (unsigned i=0;i<entities.size();i++)
	{
		const SVEntity& entity = entities[i];
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
	glLineWidth(1);

	// render icons
	rr_gl::PreserveBlend p1;
	rr_gl::PreserveBlendFunc p2;
	rr_gl::PreserveAlphaTest p3;
	rr_gl::PreserveAlphaFunc p4;
	glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.03f); // keyed icons (alpha0=transparent)
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // blended icons (alpha0=transparent)
	piwIconRectangles.clear();
	if (textureRenderer)
	{
		unsigned counter = 0;
		for (unsigned i=0;i<entities.size();i++)
		{
			rr::RRVec2 piwCenter = eye.getPositionInViewport(entities[i].position); // in -1..1 range
			rr::RRVec3 rayVisibleOrigin = eye.getRayOrigin(piwCenter) + eye.getRayDirection(piwCenter)*eye.getNear();
			rr::RRVec3 rayDir = entities[i].position-rayVisibleOrigin;
			if (eye.getDirection().dot( rayDir.normalized() )>0) // is in front of camera?
			{
				// test visibility
				bool visible = true;
				if (supercollider && ray)
				{
					ray->rayOrigin = rayVisibleOrigin;
					ray->rayDir = rayDir.normalized();
					ray->rayLengthMin = 0;
					ray->rayLengthMax = rayDir.length();
					ray->rayFlags = 0;
					ray->hitObject = NULL;
					//ray->collisionHandler = collisionHandlerFirstVisible from SVCanvas, already set
					visible = !supercollider->intersect(ray);
				}

				static rr::RRTime time;
				float brightness = entities[i].selected ? 1+fabs(fmod((float)(time.secondsPassed()),1.0f)) : (entities[i].bright?1:0.3f);
				rr::RRVec4 color(brightness,brightness,brightness,visible?1.0f:0.5f);
				float size1 = (rr::RRVec2(eye.getPositionInViewport(entities[i].position+eye.getRight()*entities[i].iconSize))-piwCenter).length()*2;
				float size2 = RR_CLAMPED(size1,0.02f,0.2f);
				rr::RRVec2 piwSize = rr::RRVec2(1,eye.getAspect()) * size2;
				rr::RRVec4 piwRectangle(piwCenter.x-piwSize.x/2,piwCenter.y-piwSize.y/2,piwSize.x,piwSize.y); // in -1..1 range
				piwIconRectangles.push_back(std::pair<const SVEntity*,rr::RRVec4>(&entities[i],piwRectangle));
				rr_gl::ToneParameters tp;
				tp.color = color;
				textureRenderer->render2D(rr_gl::getTexture(icon[entities[i].iconCode],true,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE),&tp,piwRectangle[0]*.5f+.5f,piwRectangle[1]*.5f+.5f,piwRectangle[2]*.5f,piwRectangle[3]*.5f);
			}
		}
	}
}

const SVEntity* SVEntityIcons::intersectIcons(const SVEntities& entities, rr::RRVec2 mousePositionInWindow)
{
	mousePositionInWindow.y = -mousePositionInWindow.y; // y gets inverted somewhere
	for (unsigned i=piwIconRectangles.size();i--;)
	{
		if (mousePositionInWindow.x>=piwIconRectangles[i].second.x && mousePositionInWindow.x<piwIconRectangles[i].second.x+piwIconRectangles[i].second.z &&
			mousePositionInWindow.y>=piwIconRectangles[i].second.y && mousePositionInWindow.y<piwIconRectangles[i].second.y+piwIconRectangles[i].second.w )
			return piwIconRectangles[i].first;
	}
	return NULL;
}

SVEntityIcons::~SVEntityIcons()
{
	for (unsigned i=0;i<IC_LAST;i++)
		delete icon[i];
}

}; // namespace
