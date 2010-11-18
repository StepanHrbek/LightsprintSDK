// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVLightProperties.h"
#include "SVCustomProperties.h"
#include "SVFrame.h" // updateSceneTree()

// for direction presented as altitude+azimuth
#define ANGLEX2ALT(angleX) RR_RAD2DEG(-angleX)
#define ANGLE2AZI(angle) fmod(fmod(-RR_RAD2DEG(angle),360)+360,360)
#define ALT2ANGLEX(alt) RR_DEG2RAD(-alt)
#define AZI2ANGLE(azi) RR_DEG2RAD(360-azi)

namespace rr_gl
{

SVLightProperties::SVLightProperties(SVFrame* _svframe)
	: SVProperties(_svframe)
{
	rtlight = NULL;
}

void SVLightProperties::setLight(RealtimeLight* _rtlight, int _precision)
{
	rtlight = _rtlight;

	if (!rtlight)
	{
		Clear();
	}
	else
	{
		rr::RRLight* light = &_rtlight->getRRLight();
		Clear();

		// light name
		{
			Append(propName = new wxStringProperty(wxT("Name"),wxPG_LABEL,light->name.c_str()));
		}

		// enabled
		{
			propEnabled = new BoolRefProperty(wxT("Enabled"), "Disabled light has no effect on scene, no light, no shadows.", light->enabled);
			Append(propEnabled);
		}

		// light type
		{
			const wxChar* typeStrings[] = {wxT("Directional"),wxT("Point"),wxT("Spot"),NULL};
			const long typeValues[] = {rr::RRLight::DIRECTIONAL,rr::RRLight::POINT,rr::RRLight::SPOT};
			propType = new wxEnumProperty(wxT("Light type"), wxPG_LABEL, typeStrings, typeValues, light->type);
			Append(propType);

			propPosition = new RRVec3Property(wxT("Position (m)"),"Light source position in world space",_precision,light->position,1);
			AppendIn(propType,propPosition);

			propDirection = new RRVec3Property(wxT("Direction"),"Major light direction in world space, normalized",_precision,light->direction,0.1f);
			AppendIn(propType,propDirection);

			propAltitude = new FloatProperty("Elevation (deg)","Solar elevation angle, 90 for sun in zenith, 0 for sun on horizon, negative for sun below horizon.",ANGLEX2ALT(rtlight->getParent()->angleX),_precision,-90,90,10,false);
			AppendIn(propType,propAltitude);

			propAzimuth = new FloatProperty("Azimuth (deg)","Solar azimuth angle, 90 for east, 180 for south, 270 for west.",ANGLE2AZI(rtlight->getParent()->angle),_precision,0,360,10,true);
			AppendIn(propType,propAzimuth);

			propOuterAngle = new FloatProperty("Outer angle (deg)","Outer cone angle, angle between major direction and border direction.",RR_RAD2DEG(light->outerAngleRad),_precision,0,180,10,false);
			AppendIn(propType,propOuterAngle);

			propFallOffAngle = new FloatProperty("Fall off angle (deg)","Outer angle minus inner angle, part of outer angle where intensity falls off.",RR_RAD2DEG(light->fallOffAngleRad),_precision,0,180,10,false);
			AppendIn(propType,propFallOffAngle);

			propSpotExponent = new FloatProperty("Spot exponent","Controls attenuation curve inside fall off angle.",light->spotExponent,_precision,0,1000,0.1f,false);
			AppendIn(propType,propSpotExponent);
		}

		// color
		{
			propColor = new HDRColorProperty(wxT("Color"),"Light color and intensity.",_precision,light->color);
			Append(propColor);
		}
		{
			propTexture = new ImageFileProperty(wxT("Projected texture or video"),"Texture or video projected by light. Both color and texture are applied. Type in c@pture to project live video input.");
			updateString(propTexture,getTextureDescription(light->rtProjectedTexture));
			propTexture->updateIcon(light->rtProjectedTexture);
			Append(propTexture);
			//SetPropertyAttribute( wxT("FileProperty"), wxPG_FILE_WILDCARD, wxT("All files (*.*)|*.*") );

			propTextureChangeAffectsGI = new wxBoolProperty(wxT("Realtime GI from projected video"), wxPG_LABEL, rtlight->changesInProjectedTextureAffectGI);
			AppendIn(propTexture,propTextureChangeAffectsGI);
			SetPropertyEditor(propTextureChangeAffectsGI,wxPGEditor_CheckBox);
		}

		// distance attenuation
		{
			const wxChar* attenuationStrings[] = {wxT("none"),wxT("realistic"),wxT("polynomial"),wxT("exponential"),NULL};
			const long attenuationValues[] = {rr::RRLight::NONE,rr::RRLight::PHYSICAL,rr::RRLight::POLYNOMIAL,rr::RRLight::EXPONENTIAL};
			propDistanceAttType = new wxEnumProperty(wxT("Distance attenuation type"), wxPG_LABEL, attenuationStrings, attenuationValues, light->distanceAttenuationType);
			Append(propDistanceAttType);

			propConstant = new FloatProperty("Constant","One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)",light->polynom[0],_precision,0,1e10f,0.1f,false);
			AppendIn(propDistanceAttType,propConstant);

			propLinear = new FloatProperty("Linear","One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)",light->polynom[1],_precision,0,1e10f,0.1f,false);
			AppendIn(propDistanceAttType,propLinear);

			propQuadratic = new FloatProperty("Quadratic","One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)",light->polynom[2],_precision,0,1e10f,0.1f,false);
			AppendIn(propDistanceAttType,propQuadratic);

			propClamp = new FloatProperty("Clamp","One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)",light->polynom[3],_precision,0,1,0.01f,false);
			AppendIn(propDistanceAttType,propClamp);

			propRadius = new FloatProperty("Radius (m)","One of coefficients in selected distance attenuation function pow(MAX(0,1-(distance/radius)^2),exponent)",light->radius,_precision,0,1e10f,1,false);
			AppendIn(propDistanceAttType,propRadius);

			propFallOffExponent = new FloatProperty("Exponent","One of coefficients in selected distance attenuation function pow(MAX(0,1-(distance/radius)^2),exponent)",light->fallOffExponent,_precision,0,100,0.1f,false);
			AppendIn(propDistanceAttType,propFallOffExponent);
		}

		// shadows
		{
			propCastShadows = new BoolRefProperty(wxT("Cast shadows"), "Shadows add realism, but reduce speed.", light->castShadows);
			Append(propCastShadows);
			
			propShadowmaps = new FloatProperty("Shadowmaps","Number of shadowmaps, more=higher quality, slower.",light->rtNumShadowmaps,0,1,3,10,false);
			AppendIn(propCastShadows,propShadowmaps);

			propShadowmapRes = new wxIntProperty(wxT("Resolution"),wxPG_LABEL,rtlight->getShadowmapSize());
			AppendIn(propCastShadows,propShadowmapRes);

			propShadowSamples = new wxIntProperty(wxT("Shadow Samples"),wxPG_LABEL,rtlight->getNumShadowSamples());
			AppendIn(propCastShadows,propShadowSamples);

			propNear = new FloatProperty("Near (m)","Near plane distance for generating shadowmaps. Greater value reduces shadow bias.",rtlight->getParent()->getNear(),_precision,0,1e10f,0.1f,false);
			AppendIn(propCastShadows,propNear);

			propFar = new FloatProperty("Far (m)","Far plane distance for generating shadowmaps.",rtlight->getParent()->getFar(),_precision,0,1e10f,1,false);
			AppendIn(propCastShadows,propFar);
		}

		updateHide();
	}
}

//! Must not be called in every frame, float property that is unhid in every frame loses focus immediately after click, can't be edited.
void SVLightProperties::updateHide()
{
	rr::RRLight* light = &rtlight->getRRLight();
	propPosition->Hide(light->type==rr::RRLight::DIRECTIONAL,false);
	propDirection->Hide(light->type==rr::RRLight::POINT,false);
	propAltitude->Hide(light->type!=rr::RRLight::DIRECTIONAL,false);
	propAzimuth->Hide(light->type!=rr::RRLight::DIRECTIONAL,false);
	propOuterAngle->Hide(light->type!=rr::RRLight::SPOT,false);
	propRadius->Hide(light->distanceAttenuationType!=rr::RRLight::EXPONENTIAL,false);
	propTexture->Hide(light->type!=rr::RRLight::SPOT,false);
	propTextureChangeAffectsGI->Hide(!light->rtProjectedTexture,false);
	propDistanceAttType->Hide(light->type==rr::RRLight::DIRECTIONAL,false);
	propConstant->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propLinear->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propQuadratic->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propClamp->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propFallOffExponent->Hide(light->distanceAttenuationType!=rr::RRLight::EXPONENTIAL,false);
	propFallOffAngle->Hide(light->type!=rr::RRLight::SPOT,false);
	propSpotExponent->Hide(light->type!=rr::RRLight::SPOT,false);
	propShadowmaps->Hide(!light->castShadows || light->type!=rr::RRLight::DIRECTIONAL,false);
	propShadowmapRes->Hide(!light->castShadows,false);
	propShadowSamples->Hide(!light->castShadows,false);
	propNear->Hide(!light->castShadows,false);
	propFar->Hide(!light->castShadows,false);
}

void SVLightProperties::updatePosDir()
{
	if (rtlight)
	{
		unsigned numChanges =
			updateFloat(propNear,rtlight->getParent()->getNear()) +
			updateFloat(propFar,rtlight->getParent()->getFar()) +
			updateProperty(propPosition,rtlight->getParent()->pos) +
			updateProperty(propDirection,rtlight->getParent()->dir) +
			updateFloat(propAltitude,ANGLEX2ALT(rtlight->getParent()->angleX)) +
			updateFloat(propAzimuth,ANGLE2AZI(rtlight->getParent()->angle))
			;
		if (numChanges)
		{
			Refresh();
		}
	}
}

void SVLightProperties::OnIdle(wxIdleEvent& event)
{
	// update light properties dialog
	if (IsShown())
	{
		updatePosDir();
	}
}

void SVLightProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	rr::RRLight* light = &rtlight->getRRLight();

	wxPGProperty *property = event.GetProperty();
	if (property==propName)
	{
		light->name = property->GetValue().GetString().c_str();
		svframe->updateSceneTree();
	}
	else
	if (property==propType)
	{
		//!!! nemutable svetla to ignorujou
		// change type
		light->type = (rr::RRLight::Type)property->GetValue().GetInteger();
		// change dirlight attenuation to NONE
		if (light->type==rr::RRLight::DIRECTIONAL)
		{
			light->distanceAttenuationType = rr::RRLight::NONE;
			// update displayed distance attenuation (dirlight disables it)
			propDistanceAttType->SetValue(wxVariant(light->distanceAttenuationType));
		}
		// show/hide properties
		updateHide();
	}
	else
	if (property==propPosition)
	{
		light->position << property->GetValue();
	}
	else
	if (property==propDirection)
	{
		light->direction << property->GetValue();
	}
	else
	if (property==propAltitude || property==propAzimuth)
	{
		float angleX = ALT2ANGLEX(propAltitude->GetValue().GetDouble());
		float angle = AZI2ANGLE(propAzimuth->GetValue().GetDouble());
		light->direction[0] = sin(angle)*cos(angleX);
		light->direction[1] = sin(angleX);
		light->direction[2] = cos(angle)*cos(angleX);
	}
	else
	if (property==propOuterAngle)
	{
		light->outerAngleRad = RR_DEG2RAD(property->GetValue().GetDouble());
	}
	else
	if (property==propRadius)
	{
		light->radius = property->GetValue().GetDouble();
	}
	else
	if (property==propColor)
	{
		light->color << property->GetValue();
	}
	else
	if (property==propTexture)
	{
		propTexture->updateBufferAndIcon(light->rtProjectedTexture,svs.playVideos);
		updateHide();
	}
	else
	if (property==propTextureChangeAffectsGI)
	{
		rtlight->changesInProjectedTextureAffectGI = property->GetValue().GetBool();
	}
	else
	if (property==propDistanceAttType)
	{
		light->distanceAttenuationType = (rr::RRLight::DistanceAttenuationType)property->GetValue().GetInteger();
		updateHide();
	}
	else
	if (property==propConstant)
	{
		light->polynom[0] = property->GetValue().GetDouble();
	}
	else
	if (property==propLinear)
	{
		light->polynom[1] = property->GetValue().GetDouble();
	}
	else
	if (property==propQuadratic)
	{
		light->polynom[2] = property->GetValue().GetDouble();
	}
	else
	if (property==propClamp)
	{
		light->polynom[3] = property->GetValue().GetDouble();
	}
	else
	if (property==propFallOffExponent)
	{
		light->fallOffExponent = property->GetValue().GetDouble();
	}
	else
	if (property==propFallOffAngle)
	{
		light->fallOffAngleRad = RR_DEG2RAD(property->GetValue().GetDouble());
	}
	else
	if (property==propCastShadows)
	{
		updateHide();
	}
	else
	if (property==propSpotExponent)
	{
		light->spotExponent = property->GetValue().GetDouble();
	}
	else
	if (property==propShadowmaps)
	{
		light->rtNumShadowmaps = property->GetValue().GetInteger();
	}
	else
	if (property==propShadowmapRes)
	{
		rtlight->setShadowmapSize(property->GetValue().GetInteger());
	}
	else
	if (property==propShadowSamples)
	{
		rtlight->setNumShadowSamples(property->GetValue().GetInteger());
	}
	else
	if (property==propNear)
	{
		rtlight->getParent()->setNear(property->GetValue().GetDouble());
	}
	else
	if (property==propFar)
	{
		rtlight->getParent()->setFar(property->GetValue().GetDouble());
	}
	//rtlight->dirtyShadowmap = true;
	//rtlight->dirtyGI = true;
	rtlight->updateAfterRRLightChanges();
}

BEGIN_EVENT_TABLE(SVLightProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVLightProperties::OnPropertyChange)
	EVT_IDLE(SVLightProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
