// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVLightProperties.h"
#include "SVMaterialProperties.h" // getTextureDescription
#include "SVCustomProperties.h"
#include "SVFrame.h" // updateSceneTree()

namespace rr_gl
{

SVLightProperties::SVLightProperties( wxWindow* parent )
	: wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER )
{
	SV_SET_PG_COLORS;
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

		// light type
		{
			const wxChar* typeStrings[] = {wxT("Directional"),wxT("Point"),wxT("Spot"),NULL};
			const long typeValues[] = {rr::RRLight::DIRECTIONAL,rr::RRLight::POINT,rr::RRLight::SPOT};
			propType = new wxEnumProperty(wxT("Light type"), wxPG_LABEL, typeStrings, typeValues, light->type);
			Append(propType);

			propPosition = new RRVec3Property(wxT("Position"),wxPG_LABEL,_precision,light->position);
			AppendIn(propType,propPosition);

			propDirection = new RRVec3Property(wxT("Direction"),wxPG_LABEL,_precision,light->direction);
			AppendIn(propType,propDirection);

			propOuterAngleRad = new wxFloatProperty(wxT("Outer angle (rad)"),wxPG_LABEL,light->outerAngleRad);
			propOuterAngleRad->SetAttribute("Precision",_precision);
			AppendIn(propType,propOuterAngleRad);

			propFallOffAngleRad = new wxFloatProperty(wxT("Fall off angle (rad)"),wxPG_LABEL,light->fallOffAngleRad);
			propFallOffAngleRad->SetAttribute("Precision",_precision);
			AppendIn(propType,propFallOffAngleRad);

			propSpotExponent = new wxFloatProperty(wxT("Spot exponent"),wxPG_LABEL,light->spotExponent);
			AppendIn(propType,propSpotExponent);
		}

		// color
		{
			propColor = new HDRColorProperty(wxT("Color"),wxPG_LABEL,_precision,light->color);
			Append(propColor);
		}
		{
			propTexture = new wxFileProperty(wxT("Projected texture"), wxPG_LABEL, getTextureDescription(light->rtProjectedTexture));
			Append(propTexture);
			//SetPropertyAttribute( wxT("FileProperty"), wxPG_FILE_WILDCARD, wxT("All files (*.*)|*.*") );
		}

		// distance attenuation
		{
			const wxChar* attenuationStrings[] = {wxT("none"),wxT("realistic"),wxT("polynomial"),wxT("exponential"),NULL};
			const long attenuationValues[] = {rr::RRLight::NONE,rr::RRLight::PHYSICAL,rr::RRLight::POLYNOMIAL,rr::RRLight::EXPONENTIAL};
			propDistanceAttType = new wxEnumProperty(wxT("Distance attenuation type"), wxPG_LABEL, attenuationStrings, attenuationValues, light->distanceAttenuationType);
			Append(propDistanceAttType);

			propConstant = new wxFloatProperty(wxT("Constant"),wxPG_LABEL,light->polynom[0]);
			AppendIn(propDistanceAttType,propConstant);

			propLinear = new wxFloatProperty(wxT("Linear"),wxPG_LABEL,light->polynom[1]);
			AppendIn(propDistanceAttType,propLinear);

			propQuadratic = new wxFloatProperty(wxT("Quadratic"),wxPG_LABEL,light->polynom[2]);
			AppendIn(propDistanceAttType,propQuadratic);

			propClamp = new wxFloatProperty(wxT("Clamp"),wxPG_LABEL,light->polynom[3]);
			AppendIn(propDistanceAttType,propClamp);

			propRadius = new wxFloatProperty(wxT("Radius"),wxPG_LABEL,light->radius);
			AppendIn(propDistanceAttType,propRadius);

			propFallOffExponent = new wxFloatProperty(wxT("Exponent"),wxPG_LABEL,light->fallOffExponent);
			AppendIn(propDistanceAttType,propFallOffExponent);
		}

		// shadows
		{
			propCastShadows = new wxBoolProperty(wxT("Cast shadows"), wxPG_LABEL, light->castShadows);
			Append(propCastShadows);
			SetPropertyEditor(propCastShadows,wxPGEditor_CheckBox);

			propShadowmapRes = new wxIntProperty(wxT("Resolution"),wxPG_LABEL,rtlight->getShadowmapSize());
			AppendIn(propCastShadows,propShadowmapRes);

			propShadowSamples = new wxIntProperty(wxT("Shadow Samples"),wxPG_LABEL,rtlight->getNumShadowSamples());
			AppendIn(propCastShadows,propShadowSamples);

			propNear = new wxFloatProperty(wxT("Near"),wxPG_LABEL,rtlight->getParent()->getNear());
			propNear->SetAttribute("Precision",_precision);
			AppendIn(propCastShadows,propNear);

			propFar = new wxFloatProperty(wxT("Far"),wxPG_LABEL,rtlight->getParent()->getFar());
			propFar->SetAttribute("Precision",_precision);
			AppendIn(propCastShadows,propFar);

			propOrthoSize = new wxIntProperty(wxT("Max shadow size"),wxPG_LABEL,light->rtMaxShadowSize);
			AppendIn(propCastShadows,propOrthoSize);
		}

		updateHide();
	}
}

void SVLightProperties::updateHide()
{
	rr::RRLight* light = &rtlight->getRRLight();
	propPosition->Hide(light->type==rr::RRLight::DIRECTIONAL,false);
	propDirection->Hide(light->type==rr::RRLight::POINT,false);
	propOuterAngleRad->Hide(light->type!=rr::RRLight::SPOT,false);
	propRadius->Hide(light->distanceAttenuationType!=rr::RRLight::EXPONENTIAL,false);
	propTexture->Hide(light->type!=rr::RRLight::SPOT,false);
	propDistanceAttType->Hide(light->type==rr::RRLight::DIRECTIONAL,false);
	propConstant->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propLinear->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propQuadratic->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propClamp->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propFallOffExponent->Hide(light->distanceAttenuationType!=rr::RRLight::EXPONENTIAL,false);
	propFallOffAngleRad->Hide(light->type!=rr::RRLight::SPOT,false);
	propSpotExponent->Hide(light->type!=rr::RRLight::SPOT,false);
	propShadowmapRes->Hide(!light->castShadows,false);
	propShadowSamples->Hide(!light->castShadows,false);
	propNear->Hide(!light->castShadows,false);
	propFar->Hide(!light->castShadows,false);
	propOrthoSize->Hide(light->type!=rr::RRLight::DIRECTIONAL,false);
}

void SVLightProperties::updatePosDir()
{
	if (rtlight)
	{
		unsigned numChanges =
			updateFloat(propNear,rtlight->getParent()->getNear()) +
			updateFloat(propFar,rtlight->getParent()->getFar()) +
			updateProperty(propPosition,rtlight->getParent()->pos) +
			updateProperty(propDirection,rtlight->getParent()->dir)
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
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		frame->updateSceneTree();
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
	if (property==propOuterAngleRad)
	{
		light->outerAngleRad = property->GetValue().GetDouble();
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
		delete light->rtProjectedTexture;
		light->rtProjectedTexture = rr::RRBuffer::load(property->GetValue().GetString());
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
	if (property==propFallOffAngleRad)
	{
		light->fallOffAngleRad = property->GetValue().GetDouble();
	}
	else
	if (property==propCastShadows)
	{
		light->castShadows = property->GetValue().GetBool();
		updateHide();
	}
	else
	if (property==propSpotExponent)
	{
		light->spotExponent = property->GetValue().GetDouble();
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
	else
	if (property==propOrthoSize)
	{
		light->rtMaxShadowSize = property->GetValue().GetDouble();
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
