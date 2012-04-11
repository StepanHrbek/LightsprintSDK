// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVEntityIcons.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "../PreserveState.h"
#include "wx/wx.h"

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
	float size1 = (eye.getPositionInWindow(center+eye.getRight()*iconSize)-eye.getPositionInWindow(center)).length()*2;
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

SVEntityIcons::SVEntityIcons(const char* pathToMaps, UberProgram* uberProgram)
{
	const char* filename[] = {"sv_point","sv_spot","sv_sun","sv_movement","sv_rotation","sv_scale","sv_static","sv_x","sv_y","sv_z"};
	for (IconCode i=IC_POINT;i!=IC_LAST;i=(IconCode)(i+1))
		icon[i] = rr::RRBuffer::load(RR_WX2RR(wxString::Format("%s%s.png",pathToMaps,filename[i])));

	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	programArrows = uberProgramSetup.getProgram(uberProgram);
}

bool SVEntityIcons::isOk() const
{
	for (unsigned i=0;i<IC_LAST;i++)
		if (!icon[i]) return false;
	return true;
}

void SVEntityIcons::renderIcons(const SVEntities& entities, TextureRenderer* textureRenderer, const rr::RRCamera& eye)
{
	// render arrows
	PreserveFlag p0(GL_DEPTH_TEST,false);
	programArrows->useIt();
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
	PreserveBlend p1;
	PreserveBlendFunc p2;
	PreserveAlphaTest p3;
	PreserveAlphaFunc p4;
	glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.03f); // keyed icons (alpha0=transparent)
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // blended icons (alpha0=transparent)
	piwIconRectangles.clear();
	if (textureRenderer)
	{
		unsigned counter = 0;
		for (unsigned i=0;i<entities.size();i++)
		{
			rr::RRVec2 piwCenter = eye.getPositionInWindow(entities[i].position); // in -1..1 range
			rr::RRVec3 rayVisibleOrigin = eye.getRayOrigin(piwCenter) + eye.getRayDirection(piwCenter)*eye.getNear();
			if (eye.getDirection().dot( (entities[i].position-rayVisibleOrigin).normalized() )>0) // is in front of camera?
			{
				static rr::RRTime time;
				float brightness = entities[i].selected ? 1+fabs(fmod((float)(time.secondsPassed()),1.0f)) : (entities[i].bright?1:0.3f);
				float size1 = (eye.getPositionInWindow(entities[i].position+eye.getRight()*entities[i].iconSize)-piwCenter).length()*2;
				float size2 = RR_CLAMPED(size1,0.02f,0.2f);
				rr::RRVec2 piwSize = rr::RRVec2(1,eye.getAspect()) * size2;
				rr::RRVec4 piwRectangle(piwCenter.x-piwSize.x/2,piwCenter.y-piwSize.y/2,piwSize.x,piwSize.y); // in -1..1 range
				piwIconRectangles.push_back(std::pair<const SVEntity*,rr::RRVec4>(&entities[i],piwRectangle));
				textureRenderer->render2D(getTexture(icon[entities[i].iconCode],true,false),&rr::RRVec4(brightness,brightness,brightness,1.0f),1,piwRectangle[0]*.5f+.5f,piwRectangle[1]*.5f+.5f,piwRectangle[2]*.5f,piwRectangle[3]*.5f);
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

#endif // SUPPORT_SCENEVIEWER
