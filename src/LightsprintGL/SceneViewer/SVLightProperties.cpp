// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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
			Append(propName = new wxStringProperty(_("Name"),wxPG_LABEL,RR2WX(light->name)));
		}

		// enabled
		{
			propEnabled = new BoolRefProperty(_("Enabled"), _("Disabled light has no effect on scene, no light, no shadows."), light->enabled);
			Append(propEnabled);
		}

		// light type
		{
			const wxChar* typeStrings[] = {_("Directional"),_("Point"),_("Spot"),NULL};
			const long typeValues[] = {rr::RRLight::DIRECTIONAL,rr::RRLight::POINT,rr::RRLight::SPOT};
			propType = new wxEnumProperty(_("Light type"), wxPG_LABEL, typeStrings, typeValues, light->type);
			Append(propType);

			propPosition = new RRVec3Property(_("Position")+" (m)",_("Light source position in world space"),_precision,light->position,1);
			AppendIn(propType,propPosition);

			propDirection = new RRVec3Property(_("Direction"),_("Major light direction in world space, normalized"),_precision,light->direction,0.1f);
			AppendIn(propType,propDirection);

			propAltitude = new FloatProperty(_("Elevation")+L" (\u00b0)",_("Solar elevation angle, 90 for sun in zenith, 0 for sun on horizon, negative for sun below horizon."),ANGLEX2ALT(rtlight->getParent()->angleX),_precision,-90,90,10,false);
			AppendIn(propType,propAltitude);

			propAzimuth = new FloatProperty(_("Azimuth")+L" (\u00b0)",_("Solar azimuth angle, 90 for east, 180 for south, 270 for west."),ANGLE2AZI(rtlight->getParent()->angle),_precision,0,360,10,true);
			AppendIn(propType,propAzimuth);

			propOuterAngle = new FloatProperty(_("Outer angle")+L" (\u00b0)",_("Outer cone angle, angle between major direction and border direction."),RR_RAD2DEG(light->outerAngleRad),_precision,0,180,10,false);
			AppendIn(propType,propOuterAngle);

			propFallOffAngle = new FloatProperty(_("Fall off angle")+L" (\u00b0)",_("Outer angle minus inner angle, part of outer angle where intensity falls off."),RR_RAD2DEG(light->fallOffAngleRad),_precision,0,180,10,false);
			AppendIn(propType,propFallOffAngle);

			propSpotExponent = new FloatProperty(_("Spot exponent"),_("Controls attenuation curve inside fall off angle."),light->spotExponent,_precision,0,1000,0.1f,false);
			AppendIn(propType,propSpotExponent);
		}

		// color
		{
			propColor = new HDRColorProperty(_("Color"),_("Light color and intensity."),_precision,light->color);
			Append(propColor);
		}
		{
			propTexture = new ImageFileProperty(_("Projected texture or video"),_("Texture or video projected by light. Both color and texture are applied. Type in c@pture to project live video input."));
			updateString(propTexture,getTextureDescription(light->rtProjectedTexture));
			propTexture->updateIcon(light->rtProjectedTexture);
			Append(propTexture);
			//SetPropertyAttribute( _("FileProperty"), wxPG_FILE_WILDCARD, _("All files")+": (*.*)|*.*" );

			propTextureChangeAffectsGI = new wxBoolProperty(_("Realtime GI from projected video"), wxPG_LABEL, rtlight->changesInProjectedTextureAffectGI);
			AppendIn(propTexture,propTextureChangeAffectsGI);
			SetPropertyEditor(propTextureChangeAffectsGI,wxPGEditor_CheckBox);
		}

		// distance attenuation
		{
			const wxChar* attenuationStrings[] = {_("none"),_("realistic"),_("polynomial"),_("exponential"),NULL};
			const long attenuationValues[] = {rr::RRLight::NONE,rr::RRLight::PHYSICAL,rr::RRLight::POLYNOMIAL,rr::RRLight::EXPONENTIAL};
			propDistanceAttType = new wxEnumProperty(_("Distance attenuation type"), wxPG_LABEL, attenuationStrings, attenuationValues, light->distanceAttenuationType);
			Append(propDistanceAttType);

			propConstant = new FloatProperty(_("Constant"),_("One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)"),light->polynom[0],_precision,0,1e10f,0.1f,false);
			AppendIn(propDistanceAttType,propConstant);

			propLinear = new FloatProperty(_("Linear"),_("One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)"),light->polynom[1],_precision,0,1e10f,0.1f,false);
			AppendIn(propDistanceAttType,propLinear);

			propQuadratic = new FloatProperty(_("Quadratic"),_("One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)"),light->polynom[2],_precision,0,1e10f,0.1f,false);
			AppendIn(propDistanceAttType,propQuadratic);

			propClamp = new FloatProperty(_("Clamp"),_("One of coefficients in selected distance attenuation function 1/MAX(constant+linear*distance+quadratic*distance^2,clamp)"),light->polynom[3],_precision,0,1,0.01f,false);
			AppendIn(propDistanceAttType,propClamp);

			propRadius = new FloatProperty(_("Radius")+" (m)",_("One of coefficients in selected distance attenuation function pow(MAX(0,1-(distance/radius)^2),exponent)"),light->radius,_precision,0,1e10f,1,false);
			AppendIn(propDistanceAttType,propRadius);

			propFallOffExponent = new FloatProperty(_("Exponent"),_("One of coefficients in selected distance attenuation function pow(MAX(0,1-(distance/radius)^2),exponent)"),light->fallOffExponent,_precision,0,100,0.1f,false);
			AppendIn(propDistanceAttType,propFallOffExponent);
		}

		// shadows
		{
			propCastShadows = new BoolRefProperty(_("Cast shadows"), _("Shadows add realism, but reduce speed."), light->castShadows);
			Append(propCastShadows);
			
			const wxChar* tsStrings[] = {_("0-bit (opaque shadows)"),_("1-bit (alpha keyed shadows)"),_("24-bit (rgb shadows)"),NULL};
			const long tsValues[] = {RealtimeLight::FULLY_OPAQUE_SHADOWS,RealtimeLight::ALPHA_KEYED_SHADOWS,RealtimeLight::RGB_SHADOWS};
			propShadowTransparency = new wxEnumProperty(_("Shadow transparency"),wxPG_LABEL,tsStrings,tsValues);
			propShadowTransparency->SetHelpString(_("How shadows actually work. Can be controlled via Scene properties / GI quality / Shadow transparency."));
//			propShadowTransparency->Enable(false); // not yet in wx 2.9.1
			AppendIn(propCastShadows,propShadowTransparency);
			DisableProperty(propShadowTransparency);
			
			propShadowmaps = new FloatProperty(_("Shadowmaps"),_("Number of shadowmaps, more=higher quality, slower."),light->rtNumShadowmaps,0,1,3,10,false);
			AppendIn(propCastShadows,propShadowmaps);

			propShadowmapRes = new wxIntProperty(_("Resolution"),wxPG_LABEL,rtlight->getRRLight().rtShadowmapSize);
			AppendIn(propCastShadows,propShadowmapRes);

			propShadowSamples = new wxIntProperty(_("Shadow Samples"),wxPG_LABEL,rtlight->getNumShadowSamples());
			AppendIn(propCastShadows,propShadowSamples);

			propNear = new FloatProperty(_("Near")+" (m)",_("Near plane distance for generating shadowmaps. Greater value reduces shadow bias."),rtlight->getParent()->getNear(),_precision,0,1e10f,0.1f,false);
			AppendIn(propCastShadows,propNear);

			propFar = new FloatProperty(_("Far")+" (m)",_("Far plane distance for generating shadowmaps."),rtlight->getParent()->getFar(),_precision,0,1e10f,1,false);
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
	propShadowTransparency->Hide(!light->castShadows,false);
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
			updateFloat(propAzimuth,ANGLE2AZI(rtlight->getParent()->angle)) +
			updateInt(propShadowTransparency,rtlight->shadowTransparencyActual)
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
		light->name = WX2RR(property->GetValue().GetString());
		svframe->updateSceneTree();
	}
	else
	if (property==propType)
	{
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
