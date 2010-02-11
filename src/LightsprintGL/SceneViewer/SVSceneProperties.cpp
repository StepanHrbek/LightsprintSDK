// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneProperties.h"
#include "SVCustomProperties.h"

#include "SVFrame.h" // solver access
#include "SVCanvas.h" // solver access

namespace rr_gl
{

SVSceneProperties::SVSceneProperties(wxWindow* parent, SceneViewerStateEx& _svs)
	: svs(_svs), wxPropertyGrid( parent, wxID_ANY, wxDefaultPosition, wxSize(300,400), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER )
{
	SV_SET_PG_COLORS;
	wxColour headerColor(230,230,230);

	// camera
	{
		propCamera = new wxStringProperty(wxT("Camera"), wxPG_LABEL);
		Append(propCamera);
		SetPropertyReadOnly(propCamera,true,wxPG_DONT_RECURSE);


		propCameraSpeed = new wxFloatProperty(wxT("Speed (m/s)"), wxPG_LABEL, svs.cameraMetersPerSecond);
		propCameraSpeed->SetAttribute("Precision",svs.precision);
		AppendIn(propCamera,propCameraSpeed);

		propCameraPosition = new RRVec3Property(wxT("Position"),wxPG_LABEL,svs.precision,svs.eye.pos);
		AppendIn(propCamera,propCameraPosition);

		propCameraDirection = new RRVec3Property(wxT("Direction"),wxPG_LABEL,svs.precision,svs.eye.dir);
		AppendIn(propCamera,propCameraDirection);
		SetPropertyReadOnly(propCameraDirection,true);

		propCameraAngles = new RRVec3Property(wxT("Angles"),wxPG_LABEL,svs.precision,RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle));
		AppendIn(propCamera,propCameraAngles);

		propCameraFov = new wxFloatProperty(wxT("FOV vertical degrees"), wxPG_LABEL, svs.eye.getFieldOfViewVerticalDeg());
		AppendIn(propCamera,propCameraFov);

		propCameraNear = new wxFloatProperty(wxT("Near"), wxPG_LABEL, svs.eye.getNear());
		propCameraNear->SetAttribute("Precision",svs.precision);
		AppendIn(propCamera,propCameraNear);
		SetPropertyReadOnly(propCameraNear,true);

		propCameraFar = new wxFloatProperty(wxT("Far"), wxPG_LABEL, svs.eye.getFar());
		propCameraFar->SetAttribute("Precision",svs.precision);
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
		propToneMappingBrightness->SetAttribute("Precision",svs.precision);
		AppendIn(propToneMapping,propToneMappingBrightness);

		propToneMappingContrast = new wxFloatProperty(wxT("Contrast"),wxPG_LABEL,svs.gamma);
		propToneMappingContrast->SetAttribute("Precision",svs.precision);
		AppendIn(propToneMapping,propToneMappingContrast);

		SetPropertyBackgroundColour(propToneMapping,headerColor,false);
	}

	// water
	{
		propWater = new wxBoolProperty(wxT("Water"), wxPG_LABEL, svs.renderWater);
		Append(propWater);
		SetPropertyEditor(propWater,wxPGEditor_CheckBox);

		propWaterColor = new HDRColorProperty(wxT("Color"),wxPG_LABEL,svs.precision,svs.waterColor);
		AppendIn(propWater,propWaterColor);

		propWaterLevel = new wxFloatProperty(wxT("Level"),wxPG_LABEL,svs.waterLevel);
		propWaterLevel->SetAttribute("Precision",svs.precision);
		AppendIn(propWater,propWaterLevel);

		SetPropertyBackgroundColour(propWater,headerColor,false);
	}

	// GI
	{
		propGI = new wxStringProperty(wxT("GI"), wxPG_LABEL);
		Append(propGI);
		SetPropertyReadOnly(propGI,true,wxPG_DONT_RECURSE);

		propGIEmisMultiplier = new wxFloatProperty(wxT("Emissive multiplier"),wxPG_LABEL,svs.emissiveMultiplier);
		propGIEmisMultiplier->SetAttribute("Precision",svs.precision);
		AppendIn(propGI,propGIEmisMultiplier);

		propGIEmisVideoAffectsGI = new wxBoolProperty(wxT("Emissive video realtime GI"), wxPG_LABEL, svs.videoEmittanceAffectsGI);
		AppendIn(propGI,propGIEmisVideoAffectsGI);
		SetPropertyEditor(propGIEmisVideoAffectsGI,wxPGEditor_CheckBox);

		propGITranspVideoAffectsGI = new wxBoolProperty(wxT("Transparency video realtime GI"), wxPG_LABEL, svs.videoTransmittanceAffectsGI);
		AppendIn(propGI,propGITranspVideoAffectsGI);
		SetPropertyEditor(propGITranspVideoAffectsGI,wxPGEditor_CheckBox);
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

void SVSceneProperties::updateProperties()
{
	unsigned numChanges =
		+ updateFloat(propCameraSpeed,svs.cameraMetersPerSecond)
		+ updateProperty(propCameraPosition,svs.eye.pos)
		+ updateProperty(propCameraAngles,RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle))
		+ updateFloat(propCameraFov,svs.eye.getFieldOfViewVerticalDeg())
		+ updateFloat(propCameraNear,svs.eye.getNear())
		+ updateFloat(propCameraFar,svs.eye.getFar())
		+ updateBool(propToneMapping,svs.renderTonemapping)
		+ updateBool(propToneMappingAutomatic,svs.adjustTonemapping)
		+ updateFloat(propToneMappingBrightness,svs.brightness[0])
		+ updateFloat(propToneMappingContrast,svs.gamma)
		+ updateBool(propWater,svs.renderWater)
		+ updateProperty(propWaterColor,svs.waterColor)
		+ updateFloat(propWaterLevel,svs.waterLevel)
		+ updateFloat(propGIEmisMultiplier,svs.emissiveMultiplier)
		+ updateBool(propGIEmisVideoAffectsGI,svs.videoEmittanceAffectsGI)
		+ updateBool(propGITranspVideoAffectsGI,svs.videoTransmittanceAffectsGI)
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
		svs.eye.pos << property->GetValue();
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
		svs.waterColor << property->GetValue();
	}
	else
	if (property==propWaterLevel)
	{
		svs.waterLevel = property->GetValue().GetDouble();
	}
	else
	if (property==propGIEmisMultiplier)
	{
		svs.emissiveMultiplier = property->GetValue().GetDouble();
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		frame->m_canvas->solver->setEmittance(svs.emissiveMultiplier,16,true);
	}
	else
	if (property==propGIEmisVideoAffectsGI)
	{
		svs.videoEmittanceAffectsGI = property->GetValue().GetBool();
	}
	else
	if (property==propGITranspVideoAffectsGI)
	{
		svs.videoTransmittanceAffectsGI = property->GetValue().GetBool();
	}
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
