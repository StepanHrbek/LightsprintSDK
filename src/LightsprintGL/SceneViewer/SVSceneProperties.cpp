// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVSceneProperties.h"

#ifdef SUPPORT_SCENEVIEWER

namespace rr_gl
{

SVSceneProperties::SVSceneProperties(wxWindow* parent, SceneViewerStateEx& _svs)
	: svs(_svs), wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER )
{
	wxColour headerColor(230,230,230);

	// camera
	{
		propCamera = new wxStringProperty(wxT("Camera"), wxPG_LABEL);
		Append(propCamera);
		SetPropertyReadOnly(propCamera,true,wxPG_DONT_RECURSE);

		propCameraSpeed = new wxFloatProperty(wxT("Speed (m/s)"), wxPG_LABEL, svs.cameraMetersPerSecond);
		AppendIn(propCamera,propCameraSpeed);

		propCameraPosition = new wxStringProperty(wxT("Position"),wxPG_LABEL,wxT("<composed>"));
		AppendIn(propCamera,propCameraPosition);
		AppendIn(propCameraPosition,new wxFloatProperty(wxT("x"),wxPG_LABEL,svs.eye.pos[0]));
		AppendIn(propCameraPosition,new wxFloatProperty(wxT("y"),wxPG_LABEL,svs.eye.pos[1]));
		AppendIn(propCameraPosition,new wxFloatProperty(wxT("z"),wxPG_LABEL,svs.eye.pos[2]));
		Collapse(propCameraPosition);

		propCameraDirection = new wxStringProperty(wxT("Direction"),wxPG_LABEL,wxT("<composed>"));
		AppendIn(propCamera,propCameraDirection);
		AppendIn(propCameraDirection,new wxFloatProperty(wxT("x"),wxPG_LABEL,svs.eye.dir[0]));
		AppendIn(propCameraDirection,new wxFloatProperty(wxT("y"),wxPG_LABEL,svs.eye.dir[1]));
		AppendIn(propCameraDirection,new wxFloatProperty(wxT("z"),wxPG_LABEL,svs.eye.dir[2]));
		Collapse(propCameraDirection);
		SetPropertyReadOnly(propCameraDirection,true);

		propCameraAngles = new wxStringProperty(wxT("Angle"),wxPG_LABEL,wxT("<composed>"));
		AppendIn(propCamera,propCameraAngles);
		AppendIn(propCameraAngles,new wxFloatProperty(wxT("a"),wxPG_LABEL,svs.eye.angle));
		AppendIn(propCameraAngles,new wxFloatProperty(wxT("b"),wxPG_LABEL,svs.eye.angleX));
		AppendIn(propCameraAngles,new wxFloatProperty(wxT("c"),wxPG_LABEL,svs.eye.leanAngle));
		Collapse(propCameraAngles);

		propCameraFov = new wxFloatProperty(wxT("FOV vertical degrees"), wxPG_LABEL, svs.eye.getFieldOfViewVerticalDeg());
		AppendIn(propCamera,propCameraFov);

		propCameraNear = new wxFloatProperty(wxT("Near"), wxPG_LABEL, svs.eye.getNear());
		AppendIn(propCamera,propCameraNear);
		SetPropertyReadOnly(propCameraNear,true);
		propCameraFar = new wxFloatProperty(wxT("Far"), wxPG_LABEL, svs.eye.getFar());
		AppendIn(propCamera,propCameraFar);
		SetPropertyReadOnly(propCameraFar,true);

		SetPropertyBackgroundColour(propCamera,headerColor,false);
	}

	// tone mapping
	{
		propToneMapping = new wxBoolProperty(wxT("Tone Mapping"), wxPG_LABEL, svs.renderTonemapping);
		Append(propToneMapping);
		SetPropertyEditor(propToneMapping,wxPGEditor_CheckBox);

		propToneMappingAutomatic = new wxBoolProperty(wxT("Automatic"), wxPG_LABEL, svs.adjustTonemapping);
		AppendIn(propToneMapping,propToneMappingAutomatic);
		SetPropertyEditor(propToneMappingAutomatic,wxPGEditor_CheckBox);

		propToneMappingBrightness = new wxFloatProperty(wxT("Brightness"),wxPG_LABEL,svs.brightness[0]);
		AppendIn(propToneMapping,propToneMappingBrightness);

		propToneMappingContrast = new wxFloatProperty(wxT("Contrast"),wxPG_LABEL,svs.gamma);
		AppendIn(propToneMapping,propToneMappingContrast);

		SetPropertyBackgroundColour(propToneMapping,headerColor,false);
	}

	// water
	{
		propWater = new wxBoolProperty(wxT("Water"), wxPG_LABEL, svs.renderWater);
		Append(propWater);
		SetPropertyEditor(propWater,wxPGEditor_CheckBox);

		propWaterColor = new wxStringProperty(wxT("Color"),wxPG_LABEL,wxT("<composed>"));
		AppendIn(propWater,propWaterColor);
		AppendIn(propWaterColor,new wxFloatProperty(wxT("r"),wxPG_LABEL,svs.waterColor[0]));
		AppendIn(propWaterColor,new wxFloatProperty(wxT("g"),wxPG_LABEL,svs.waterColor[1]));
		AppendIn(propWaterColor,new wxFloatProperty(wxT("b"),wxPG_LABEL,svs.waterColor[2]));
		Collapse(propWaterColor);

		propWaterLevel = new wxFloatProperty(wxT("Level"),wxPG_LABEL,svs.waterLevel);
		AppendIn( propWater, propWaterLevel );

		SetPropertyBackgroundColour(propWater,headerColor,false);
	}
/*
	// size
	{
		propSizes = new wxStringProperty(wxT("Sizes"), wxPG_LABEL);
		Append(propSizes);

		propSizesSize = new wxStringProperty(wxT("Scene size"),wxPG_LABEL,wxT("..."));
		AppendIn(propSizes,propSizesSize);
		propSizesMin = new wxStringProperty(wxT("Scene min"),wxPG_LABEL,wxT("..."));
		AppendIn(propSizes,propSizesMin);
		propSizesMax = new wxStringProperty(wxT("Scene max"),wxPG_LABEL,wxT("..."));
		AppendIn(propSizes,propSizesMax);

		SetPropertyReadOnly(propSizes);
		SetPropertyBackgroundColour(propSizes,headerColor,false);
	}
*/
	updateHide();
}

//! Copy svs -> hide/show property.
void SVSceneProperties::updateHide()
{
	propToneMappingAutomatic->Hide(!svs.renderTonemapping,false);
	propToneMappingBrightness->Hide(!svs.renderTonemapping,false);
	propToneMappingContrast->Hide(!svs.renderTonemapping,false);
	propWaterColor->Hide(!svs.renderWater,false);
	propWaterLevel->Hide(!svs.renderWater,false);
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

void SVSceneProperties::updateProperties()
{
	unsigned numChanges =
		+ updateFloat(propCameraSpeed,svs.cameraMetersPerSecond)
		+ updateFloat(propCameraPosition->GetPropertyByName("x"),svs.eye.pos[0])
		+ updateFloat(propCameraPosition->GetPropertyByName("y"),svs.eye.pos[1])
		+ updateFloat(propCameraPosition->GetPropertyByName("z"),svs.eye.pos[2])
		+ updateFloat(propCameraAngles->GetPropertyByName("a"),svs.eye.angle)
		+ updateFloat(propCameraAngles->GetPropertyByName("b"),svs.eye.angleX)
		+ updateFloat(propCameraAngles->GetPropertyByName("c"),svs.eye.leanAngle)
		+ updateFloat(propCameraFov,svs.eye.getFieldOfViewVerticalDeg())
		+ updateFloat(propCameraNear,svs.eye.getNear())
		+ updateFloat(propCameraFar,svs.eye.getFar())
		+ updateBool(propToneMapping,svs.renderTonemapping)
		+ updateBool(propToneMappingAutomatic,svs.adjustTonemapping)
		+ updateFloat(propToneMappingBrightness,svs.brightness[0])
		+ updateFloat(propToneMappingContrast,svs.gamma)
		+ updateBool(propWater,svs.renderWater)
		+ updateFloat(propWaterColor->GetPropertyByName("r"),svs.waterColor[0])
		+ updateFloat(propWaterColor->GetPropertyByName("g"),svs.waterColor[1])
		+ updateFloat(propWaterColor->GetPropertyByName("b"),svs.waterColor[2])
		+ updateFloat(propWaterLevel,svs.waterLevel)
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
		updateProperties();
	}
}

void SVSceneProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propCameraSpeed)
	{
		svs.cameraMetersPerSecond = property->GetValue().GetDouble();
	}
	else
	if (property==propCameraPosition)
	{
		svs.eye.pos = rr::RRVec3(
			property->GetPropertyByName("x")->GetValue().GetDouble(),
			property->GetPropertyByName("y")->GetValue().GetDouble(),
			property->GetPropertyByName("z")->GetValue().GetDouble());
	}
	else
	if (property==propCameraAngles)
	{
		svs.eye.angle = property->GetPropertyByName("a")->GetValue().GetDouble();
		svs.eye.angleX = property->GetPropertyByName("b")->GetValue().GetDouble();
		svs.eye.leanAngle = property->GetPropertyByName("c")->GetValue().GetDouble();
	}
	else
	if (property==propCameraFov)
	{
		svs.eye.setFieldOfViewVerticalDeg(property->GetValue().GetDouble());
	}
	else
	if (property==propCameraNear)
	{
		svs.eye.setNear(property->GetValue().GetDouble());
	}
	else
	if (property==propCameraFar)
	{
		svs.eye.setFar(property->GetValue().GetDouble());
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
	else
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
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
