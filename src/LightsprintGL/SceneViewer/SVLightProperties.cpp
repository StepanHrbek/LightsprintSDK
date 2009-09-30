// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVLightProperties.h"

#ifdef SUPPORT_SCENEVIEWER

//#include "SVFrame.h"

namespace rr_gl
{

SVLightProperties::SVLightProperties( wxWindow* parent )
	: wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER )
{
	rtlight = NULL;
}

void SVLightProperties::setLight(RealtimeLight* _rtlight)
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

		// light type
		{
			const wxChar* typeStrings[] = {wxT("Directional"),wxT("Point"),wxT("Spot"),NULL};
			const long typeValues[] = {rr::RRLight::DIRECTIONAL,rr::RRLight::POINT,rr::RRLight::SPOT};
			propType = new wxEnumProperty(wxT("Light type"), wxPG_LABEL, typeStrings, typeValues, light->type);
			Append(propType);

			propPosition = new wxStringProperty(wxT("Position"),wxPG_LABEL,wxT("<composed>"));
			AppendIn(propType,propPosition);
			AppendIn(propPosition,new wxFloatProperty(wxT("x"),wxPG_LABEL,light->position[0]));
			AppendIn(propPosition,new wxFloatProperty(wxT("y"),wxPG_LABEL,light->position[1]));
			AppendIn(propPosition,new wxFloatProperty(wxT("z"),wxPG_LABEL,light->position[2]));
			Collapse(propPosition);

			propDirection = new wxStringProperty(wxT("Direction"),wxPG_LABEL,wxT("<composed>"));
			AppendIn(propType,propDirection);
			AppendIn(propDirection,new wxFloatProperty(wxT("x"),wxPG_LABEL,light->direction[0]));
			AppendIn(propDirection,new wxFloatProperty(wxT("y"),wxPG_LABEL,light->direction[1]));
			AppendIn(propDirection,new wxFloatProperty(wxT("z"),wxPG_LABEL,light->direction[2]));
			Collapse(propDirection);

			propOuterAngleRad = new wxFloatProperty(wxT("Outer angle (rad)"),wxPG_LABEL,light->outerAngleRad);
			AppendIn(propType,propOuterAngleRad);

			propFallOffAngleRad = new wxFloatProperty(wxT("Fall off angle (rad)"),wxPG_LABEL,light->fallOffAngleRad);
			AppendIn(propType,propFallOffAngleRad);

			propSpotExponent = new wxFloatProperty(wxT("Spot exponent"),wxPG_LABEL,light->spotExponent);
			AppendIn(propType,propSpotExponent);
		}

		// color
		{
			propColor = new wxStringProperty(wxT("Color"),wxPG_LABEL,wxT("<composed>"));
			Append(propColor);
			AppendIn(propColor,new wxFloatProperty(wxT("r"),wxPG_LABEL,light->color[0]));
			AppendIn(propColor,new wxFloatProperty(wxT("g"),wxPG_LABEL,light->color[1]));
			AppendIn(propColor,new wxFloatProperty(wxT("b"),wxPG_LABEL,light->color[2]));
			Collapse(propColor);
		}
		{
			propTexture = new wxFileProperty(wxT("Projected texture"), wxPG_LABEL, light->rtProjectedTextureFilename);
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

			propShadowmapRes = new wxFloatProperty(wxT("Resolution"),wxPG_LABEL,rtlight->getShadowmapSize());
			AppendIn(propCastShadows,propShadowmapRes);

			propNear = new wxFloatProperty(wxT("Near"),wxPG_LABEL,rtlight->getParent()->getNear());
			AppendIn(propCastShadows,propNear);

			propFar = new wxFloatProperty(wxT("Far"),wxPG_LABEL,rtlight->getParent()->getFar());
			AppendIn(propCastShadows,propFar);

			propOrthoSize = new wxFloatProperty(wxT("Max shadow size"),wxPG_LABEL,light->rtMaxShadowSize);
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
	propNear->Hide(!light->castShadows,false);
	propFar->Hide(!light->castShadows,false);
	propOrthoSize->Hide(light->type!=rr::RRLight::DIRECTIONAL,false);
}

//! Copy light -> property (1 float).
static unsigned updateFloat(wxPGProperty* prop,float value)
{
	if (value!=(float)prop->GetValue().GetDouble())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
}

void SVLightProperties::updatePosDir()
{
	if (rtlight)
	{
		unsigned numChanges =
			updateFloat(propNear,rtlight->getParent()->getNear()) +
			updateFloat(propFar,rtlight->getParent()->getFar()) +
			updateFloat(propPosition->GetPropertyByName("x"),rtlight->getParent()->pos[0]) +
			updateFloat(propPosition->GetPropertyByName("y"),rtlight->getParent()->pos[1]) +
			updateFloat(propPosition->GetPropertyByName("z"),rtlight->getParent()->pos[2]) +
			updateFloat(propDirection->GetPropertyByName("x"),rtlight->getParent()->dir[0]) +
			updateFloat(propDirection->GetPropertyByName("y"),rtlight->getParent()->dir[1]) +
			updateFloat(propDirection->GetPropertyByName("z"),rtlight->getParent()->dir[2])
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
	if (property==propPosition)
	{
		light->position[0] = property->GetPropertyByName("x")->GetValue().GetDouble();
		light->position[1] = property->GetPropertyByName("y")->GetValue().GetDouble();
		light->position[2] = property->GetPropertyByName("z")->GetValue().GetDouble();
	}
	if (property==propDirection)
	{
		light->direction[0] = property->GetPropertyByName("x")->GetValue().GetDouble();
		light->direction[1] = property->GetPropertyByName("y")->GetValue().GetDouble();
		light->direction[2] = property->GetPropertyByName("z")->GetValue().GetDouble();
	}
	if (property==propOuterAngleRad)
	{
		light->outerAngleRad = property->GetValue().GetDouble();
	}
	if (property==propRadius)
	{
		light->radius = property->GetValue().GetDouble();
	}
	if (property==propColor)
	{
		light->color[0] = property->GetPropertyByName("r")->GetValue().GetDouble();
		light->color[1] = property->GetPropertyByName("g")->GetValue().GetDouble();
		light->color[2] = property->GetPropertyByName("b")->GetValue().GetDouble();
	}
	if (property==propTexture)
	{
		free(light->rtProjectedTextureFilename);
		light->rtProjectedTextureFilename = _strdup(property->GetValue().GetString());
	}
	if (property==propDistanceAttType)
	{
		light->distanceAttenuationType = (rr::RRLight::DistanceAttenuationType)property->GetValue().GetInteger();
		updateHide();
	}
	if (property==propConstant)
	{
		light->polynom[0] = property->GetValue().GetDouble();
	}
	if (property==propLinear)
	{
		light->polynom[1] = property->GetValue().GetDouble();
	}
	if (property==propQuadratic)
	{
		light->polynom[2] = property->GetValue().GetDouble();
	}
	if (property==propClamp)
	{
		light->polynom[3] = property->GetValue().GetDouble();
	}
	if (property==propFallOffExponent)
	{
		light->fallOffExponent = property->GetValue().GetDouble();
	}
	if (property==propFallOffAngleRad)
	{
		light->fallOffAngleRad = property->GetValue().GetDouble();
	}
	if (property==propCastShadows)
	{
		light->castShadows = property->GetValue().GetBool();
		updateHide();
	}
	if (property==propSpotExponent)
	{
		light->spotExponent = property->GetValue().GetDouble();
	}
	if (property==propShadowmapRes)
	{
		rtlight->setShadowmapSize(property->GetValue().GetInteger());
	}
	if (property==propNear)
	{
		rtlight->getParent()->setNear(property->GetValue().GetDouble());
	}
	if (property==propFar)
	{
		rtlight->getParent()->setFar(property->GetValue().GetDouble());
	}
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
