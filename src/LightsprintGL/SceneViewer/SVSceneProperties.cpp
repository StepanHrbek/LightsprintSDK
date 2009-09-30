// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVSceneProperties.h"

#ifdef SUPPORT_SCENEVIEWER

//#include "SVFrame.h"

namespace rr_gl
{

SVSceneProperties::SVSceneProperties(wxWindow* parent, SceneViewerStateEx& _svs)
	: svs(_svs), wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER )
{
	// water
	{
		propWater = new wxBoolProperty(wxT("Water"), wxPG_LABEL, svs.renderWater);
		Append(propWater);
		SetPropertyEditor(propWater,wxPGEditor_CheckBox);

		propWaterColor = new wxStringProperty(wxT("Color"),wxPG_LABEL,wxT("<composed>"));
		AppendIn( propWater, propWaterColor );
		AppendIn( propWaterColor, new wxFloatProperty(wxT("r"),wxPG_LABEL,svs.waterColor[0]) );
		AppendIn( propWaterColor, new wxFloatProperty(wxT("g"),wxPG_LABEL,svs.waterColor[1]) );
		AppendIn( propWaterColor, new wxFloatProperty(wxT("b"),wxPG_LABEL,svs.waterColor[2]) );
		Collapse( propWaterColor );

		propWaterLevel = new wxFloatProperty(wxT("Level"),wxPG_LABEL,svs.waterLevel);
		AppendIn( propWater, propWaterLevel );
	}

	// tone mapping
	{
		propToneMapping = new wxBoolProperty(wxT("Tone Mapping"), wxPG_LABEL, svs.renderTonemapping);
		Append(propToneMapping);
		SetPropertyEditor(propToneMapping,wxPGEditor_CheckBox);

		propToneMappingBrightness = new wxFloatProperty(wxT("Brightness"),wxPG_LABEL,svs.brightness[0]);
		AppendIn(propToneMapping,propToneMappingBrightness);
		propToneMappingContrast = new wxFloatProperty(wxT("Contrast"),wxPG_LABEL,svs.gamma);
		AppendIn(propToneMapping,propToneMappingContrast);
		propToneMappingAutomatic = new wxBoolProperty(wxT("Automatic"), wxPG_LABEL, svs.adjustTonemapping);
		AppendIn(propToneMapping,propToneMappingAutomatic);
		SetPropertyEditor(propToneMappingAutomatic,wxPGEditor_CheckBox);
}

	updateHide();
}

//! Copy svs -> hide/show property.
void SVSceneProperties::updateHide()
{
	propWaterColor->Hide(!svs.renderWater,false);
	propWaterLevel->Hide(!svs.renderWater,false);
	propToneMappingAutomatic->Hide(!svs.renderTonemapping,false);
	propToneMappingBrightness->Hide(!svs.renderTonemapping,false);
	propToneMappingContrast->Hide(!svs.renderTonemapping,false);
}

//! Copy light -> property (1 bool).
static unsigned updateBool(wxPGProperty* prop,bool value)
{
	if (value!=prop->GetValue().GetBool())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
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

void SVSceneProperties::updateAllProperties()
{
	unsigned numChanges =
		+ updateBool(propWater,svs.renderWater)
		+ updateFloat(propWaterColor->GetPropertyByName("r"),svs.waterColor[0])
		+ updateFloat(propWaterColor->GetPropertyByName("g"),svs.waterColor[1])
		+ updateFloat(propWaterColor->GetPropertyByName("b"),svs.waterColor[2])
		+ updateFloat(propWaterLevel,svs.waterLevel)
		+ updateBool(propToneMapping,svs.renderTonemapping)
		+ updateBool(propToneMappingAutomatic,svs.adjustTonemapping)
		+ updateFloat(propToneMappingBrightness,svs.brightness[0])
		+ updateFloat(propToneMappingContrast,svs.gamma)
		;
	if (numChanges)
	{
		Refresh();
	}
}

void SVSceneProperties::updateVolatileProperties()
{
	unsigned numChanges =
		+ updateFloat(propToneMappingBrightness,svs.brightness[0])
		;
	if (numChanges)
	{
		Refresh();
	}
}

void SVSceneProperties::OnIdle(wxIdleEvent& event)
{
	if (IsShown())
	{
		updateVolatileProperties();
	}
}

void SVSceneProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propWater)
	{
		svs.renderWater = property->GetValue().GetBool();
		updateHide();
	}
	else
	if (property==propWaterColor)
	{
		svs.waterColor[0] = property->GetPropertyByName("r")->GetValue().GetDouble();
		svs.waterColor[1] = property->GetPropertyByName("g")->GetValue().GetDouble();
		svs.waterColor[2] = property->GetPropertyByName("b")->GetValue().GetDouble();
	}
	else
	if (property==propWaterLevel)
	{
		svs.waterLevel = property->GetValue().GetDouble();
	}
	else

	if (property==propToneMapping)
	{
		svs.renderTonemapping = property->GetValue().GetBool();
		updateHide();
	}
	else
	if (property==propToneMappingAutomatic)
	{
		svs.adjustTonemapping = property->GetValue().GetBool();
	}
	else
	if (property==propToneMappingBrightness)
	{
		svs.brightness = rr::RRVec4(property->GetValue().GetDouble());
	}
	if (property==propToneMappingContrast)
	{
		svs.gamma = property->GetValue().GetDouble();
	}
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
