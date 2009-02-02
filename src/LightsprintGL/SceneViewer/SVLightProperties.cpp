// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
//
// include

#include "SVLightProperties.h"

#if wxCHECK_VERSION(2,9,0)
	// makes wxPropGrid 1.4 code compile with wxWidgets 2.9
	#define wxPGId wxPGProperty*
#endif

namespace rr_gl
{

SVLightProperties::SVLightProperties( wxWindow* parent )
		: wxDialog( parent, wxID_ANY, "Light properties", wxDefaultPosition, wxSize(300,500), wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER )
{
	rtlight = NULL;

	pg = new wxPropertyGrid( this, -1, wxPoint(0,0), GetClientSize(), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER );
	//pg->SetExtraStyle( wxPG_EX_HELP_AS_TOOLTIPS|wxPG_EX_LEGACY_VALIDATORS|wxWS_EX_VALIDATE_RECURSIVELY );

	wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );
	topsizer->Add( pg, 1, wxEXPAND|wxALL );
	topsizer->SetSizeHints( this );
	SetSizer( topsizer );
	SetSize( 330, 330 );
}

void SVLightProperties::setLight(RealtimeLight* _rtlight)
{
	rtlight = _rtlight;

	if (!rtlight)
	{
		Hide();
	}
	else
	{
		rr::RRLight* light = &_rtlight->getRRLight();
		pg->Clear();

		// light type
		{
			const wxChar* typeStrings[] = {wxT("Directional"),wxT("Point"),wxT("Spot"),NULL};
			const long typeValues[] = {rr::RRLight::DIRECTIONAL,rr::RRLight::POINT,rr::RRLight::SPOT};
			propType = new wxEnumProperty(wxT("Light type"), wxPG_LABEL, typeStrings, typeValues, light->type);
			wxPGId idType = pg->Append( propType );

			propPosition = new wxStringProperty(wxT("Position"),wxPG_LABEL,wxT("<composed>"));
			wxPGId posId = pg->AppendIn( idType, propPosition );
			wxPGId id;
			id = pg->AppendIn( posId, new wxFloatProperty(wxT("x"),wxPG_LABEL,light->position[0]) );
			id = pg->AppendIn( posId, new wxFloatProperty(wxT("y"),wxPG_LABEL,light->position[1]) );
			id = pg->AppendIn( posId, new wxFloatProperty(wxT("z"),wxPG_LABEL,light->position[2]) );
			pg->Collapse( posId );

			propDirection = new wxStringProperty(wxT("Direction"),wxPG_LABEL,wxT("<composed>"));
			wxPGId dirId = pg->AppendIn( idType, propDirection );
			pg->AppendIn( dirId, new wxFloatProperty(wxT("x"),wxPG_LABEL,light->direction[0]) );
			pg->AppendIn( dirId, new wxFloatProperty(wxT("y"),wxPG_LABEL,light->direction[1]) );
			pg->AppendIn( dirId, new wxFloatProperty(wxT("z"),wxPG_LABEL,light->direction[2]) );
			pg->Collapse( dirId );

			propOuterAngleRad = new wxFloatProperty(wxT("Outer angle (rad)"),wxPG_LABEL,light->outerAngleRad);
			pg->AppendIn( idType, propOuterAngleRad );

			propFallOffAngleRad = new wxFloatProperty(wxT("Fall off angle (rad)"),wxPG_LABEL,light->fallOffAngleRad);
			pg->AppendIn( idType, propFallOffAngleRad );

			propSpotExponent = new wxFloatProperty(wxT("Spot exponent"),wxPG_LABEL,light->spotExponent);
			pg->AppendIn( idType, propSpotExponent );
		}

		{
			propColor = new wxStringProperty(wxT("Color"),wxPG_LABEL,wxT("<composed>"));
			wxPGId colId = pg->Append( propColor );
			pg->AppendIn( colId, new wxFloatProperty(wxT("r"),wxPG_LABEL,light->color[0]) );
			pg->AppendIn( colId, new wxFloatProperty(wxT("g"),wxPG_LABEL,light->color[1]) );
			pg->AppendIn( colId, new wxFloatProperty(wxT("b"),wxPG_LABEL,light->color[2]) );
			pg->Collapse( colId );
		}

		{
			propTexture = new wxFileProperty(wxT("Projected texture"), wxPG_LABEL, light->rtProjectedTextureFilename);
			pg->Append( propTexture );
			//pg->SetPropertyAttribute( wxT("FileProperty"), wxPG_FILE_WILDCARD, wxT("All files (*.*)|*.*") );
		}

		// distance attenuation
		{
			const wxChar* attenuationStrings[] = {wxT("none"),wxT("realistic"),wxT("polynomial"),wxT("exponential"),NULL};
			const long attenuationValues[] = {rr::RRLight::NONE,rr::RRLight::PHYSICAL,rr::RRLight::POLYNOMIAL,rr::RRLight::EXPONENTIAL};
			propDistanceAttType = new wxEnumProperty(wxT("Distance attenuation type"), wxPG_LABEL, attenuationStrings, attenuationValues, light->distanceAttenuationType);
			wxPGId idDistAtt = pg->Append( propDistanceAttType );

			propConstant = new wxFloatProperty(wxT("Constant"),wxPG_LABEL,light->polynom[0]);
			pg->AppendIn( idDistAtt, propConstant );

			propLinear = new wxFloatProperty(wxT("Linear"),wxPG_LABEL,light->polynom[1]);
			pg->AppendIn( idDistAtt, propLinear );

			propQuadratic = new wxFloatProperty(wxT("Quadratic"),wxPG_LABEL,light->polynom[2]);
			pg->AppendIn( idDistAtt, propQuadratic );

			propClamp = new wxFloatProperty(wxT("Clamp"),wxPG_LABEL,light->polynom[3]);
			pg->AppendIn( idDistAtt, propClamp );

			propRadius = new wxFloatProperty(wxT("Radius"),wxPG_LABEL,light->radius);
			pg->AppendIn( idDistAtt, propRadius );

			propFallOffExponent = new wxFloatProperty(wxT("Exponent"),wxPG_LABEL,light->fallOffExponent);
			pg->AppendIn( idDistAtt, propFallOffExponent );
		}

		// shadows
		{
			propCastShadows = new wxBoolProperty(wxT("Cast shadows"), wxPG_LABEL, light->castShadows);
			wxPGId idShad = pg->Append( propCastShadows );

			propShadowmapRes = new wxFloatProperty(wxT("Resolution"),wxPG_LABEL,rtlight->getShadowmapSize());
			pg->AppendIn( idShad, propShadowmapRes );

			propNear = new wxFloatProperty(wxT("Near"),wxPG_LABEL,rtlight->getParent()->getNear());
			pg->AppendIn( idShad, propNear );

			propFar = new wxFloatProperty(wxT("Far"),wxPG_LABEL,rtlight->getParent()->getFar());
			pg->AppendIn( idShad, propFar );

			propOrthoSize = new wxFloatProperty(wxT("Ortho size"),wxPG_LABEL,rtlight->getParent()->orthoSize);
			pg->AppendIn( idShad, propOrthoSize );
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
	propTexture->Hide(light->type==rr::RRLight::DIRECTIONAL,false);
	propDistanceAttType->Hide(light->type==rr::RRLight::DIRECTIONAL,false);
	propConstant->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propLinear->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propQuadratic->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propClamp->Hide(light->distanceAttenuationType!=rr::RRLight::POLYNOMIAL,false);
	propFallOffExponent->Hide(light->distanceAttenuationType!=rr::RRLight::EXPONENTIAL,false);
	propSpotExponent->Hide(light->type!=rr::RRLight::SPOT,false);
	propFallOffAngleRad->Hide(light->type!=rr::RRLight::SPOT,false);
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
	unsigned numChanges =
		updateFloat(propPosition->GetPropertyByName("x"),rtlight->getParent()->pos[0]) +
		updateFloat(propPosition->GetPropertyByName("y"),rtlight->getParent()->pos[1]) +
		updateFloat(propPosition->GetPropertyByName("z"),rtlight->getParent()->pos[2]) +
		updateFloat(propDirection->GetPropertyByName("x"),rtlight->getParent()->dir[0]) +
		updateFloat(propDirection->GetPropertyByName("y"),rtlight->getParent()->dir[1]) +
		updateFloat(propDirection->GetPropertyByName("z"),rtlight->getParent()->dir[2]) +
		updateFloat(propNear,rtlight->getParent()->getNear()) +
		updateFloat(propFar,rtlight->getParent()->getFar()) ;
	if (numChanges)
	{
		Refresh();
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
	if (property==propSpotExponent)
	{
		light->spotExponent = property->GetValue().GetDouble();
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
		rtlight->getParent()->orthoSize = property->GetValue().GetDouble();
	}
	//rtlight->dirtyShadowmap = true;
	//rtlight->dirtyGI = true;
	rtlight->updateAfterRRLightChanges();
}

BEGIN_EVENT_TABLE(SVLightProperties, wxDialog)
	EVT_PG_CHANGED(-1,SVLightProperties::OnPropertyChange)
	EVT_IDLE(SVLightProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace
