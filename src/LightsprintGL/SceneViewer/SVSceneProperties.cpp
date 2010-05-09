// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneProperties.h"

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
		propToneMapping = new BoolRefProperty(wxT("Tone Mapping"), svs.renderTonemapping);
		Append(propToneMapping);

		propToneMappingAutomatic = new BoolRefProperty(wxT("Automatic"), svs.tonemappingAutomatic);
		AppendIn(propToneMapping,propToneMappingAutomatic);

		{
			propToneMappingAutomaticMin = new wxFloatProperty(wxT("Min Brightness"),wxPG_LABEL,svs.tonemappingAutomaticMin);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticMin);

			propToneMappingAutomaticMax = new wxFloatProperty(wxT("Max Brightness"),wxPG_LABEL,svs.tonemappingAutomaticMax);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticMax);

			propToneMappingAutomaticTarget = new wxFloatProperty(wxT("Target screen intensity"),wxPG_LABEL,svs.tonemappingAutomaticTarget);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticTarget);

			propToneMappingAutomaticSpeed = new wxFloatProperty(wxT("Speed"),wxPG_LABEL,svs.tonemappingAutomaticSpeed);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticSpeed);
		}

		propToneMappingBrightness = new wxFloatProperty(wxT("Brightness"),wxPG_LABEL,svs.tonemappingBrightness[0]);
		propToneMappingBrightness->SetAttribute("Precision",svs.precision);
		AppendIn(propToneMapping,propToneMappingBrightness);

		propToneMappingContrast = new wxFloatProperty(wxT("Contrast"),wxPG_LABEL,svs.tonemappingGamma);
		propToneMappingContrast->SetAttribute("Precision",svs.precision);
		AppendIn(propToneMapping,propToneMappingContrast);

		SetPropertyBackgroundColour(propToneMapping,headerColor,false);
	}

	// render materials
	{
		propRenderMaterials = new wxStringProperty(wxT("Render materials"), wxPG_LABEL);
		Append(propRenderMaterials);
		SetPropertyReadOnly(propRenderMaterials,true,wxPG_DONT_RECURSE);

		propRenderMaterialDiffuse = new BoolRefProperty(wxT("Diffuse color"), svs.renderMaterialDiffuse, wxT("Toggles between rendering diffuse colors and diffuse white. With diffuse color disabled, color bleeding is usually clearly visible."));
		AppendIn(propRenderMaterials,propRenderMaterialDiffuse);

		propRenderMaterialSpecular = new BoolRefProperty(wxT("Specular"), svs.renderMaterialSpecular, wxT("Toggles rendering specular reflections. Disabling them could make huge highly specular scenes render faster."));
		AppendIn(propRenderMaterials,propRenderMaterialSpecular);

		propRenderMaterialEmittance = new BoolRefProperty(wxT("Emittance"), svs.renderMaterialEmission, wxT("Toggles rendering emittance of emissive surfaces."));
		AppendIn(propRenderMaterials,propRenderMaterialEmittance);

		propRenderMaterialTransparency = new BoolRefProperty(wxT("Transparency"), svs.renderMaterialTransparency, wxT("Toggles rendering transparency of semi-transparent surfaces. Disabling it could make rendering faster."));
		AppendIn(propRenderMaterials,propRenderMaterialTransparency);

		propRenderMaterialTextures = new BoolRefProperty(wxT("Textures"), svs.renderMaterialTextures, wxT("(ctrl-t) Toggles between material textures and flat colors. Disabling textures could make rendering faster."));
		AppendIn(propRenderMaterials,propRenderMaterialTextures);

		SetPropertyBackgroundColour(propRenderMaterials,headerColor,false);
	}

	// render extras
	{
		propRenderExtras = new wxStringProperty(wxT("Render extras"), wxPG_LABEL);
		Append(propRenderExtras);
		SetPropertyReadOnly(propRenderExtras,true,wxPG_DONT_RECURSE);

		// water
		{
			propWater = new BoolRefProperty(wxT("Water"), svs.renderWater);
			AppendIn(propRenderExtras,propWater);

			propWaterColor = new HDRColorProperty(wxT("Color"),wxPG_LABEL,svs.precision,svs.waterColor);
			AppendIn(propWater,propWaterColor);

			propWaterLevel = new wxFloatProperty(wxT("Level"),wxPG_LABEL,svs.waterLevel);
			propWaterLevel->SetAttribute("Precision",svs.precision);
			AppendIn(propWater,propWaterLevel);
		}

		propRenderWireframe = new BoolRefProperty(wxT("Wireframe"), svs.renderWireframe, wxT("(ctrl-w) Toggles between solid and wireframe rendering modes."));
		AppendIn(propRenderExtras,propRenderWireframe);

		propRenderFPS = new BoolRefProperty(wxT("FPS"), svs.renderFPS, wxT("(ctrl-f) FPS counter shows number of frames rendered in last second."));
		AppendIn(propRenderExtras,propRenderFPS);

		propRenderLogo = new BoolRefProperty(wxT("Logo"), svs.renderLogo, wxT("Logo is loaded from data/maps/sv_logo.png."));
		AppendIn(propRenderExtras,propRenderLogo);

		propRenderVignettation = new BoolRefProperty(wxT("Vignettation"), svs.renderVignette, wxT("Vignette overlay is loaded from data/maps/vignette.png."));
		AppendIn(propRenderExtras,propRenderVignettation);

		// grid
		{
			propGrid = new BoolRefProperty(wxT("Grid"), svs.renderGrid, wxT("Toggles rendering 2d grid in y=0 plane, around world center."));
			AppendIn(propRenderExtras,propGrid);

			propGridNumSegments = new wxFloatProperty(wxT("Segments"),wxPG_LABEL,svs.gridNumSegments);
			propGridNumSegments->SetAttribute("Precision",svs.precision);
			propGridNumSegments->SetAttribute("Min",1);
			propGridNumSegments->SetAttribute("Max",1000);
			AppendIn(propGrid,propGridNumSegments);

			propGridSegmentSize = new wxFloatProperty(wxT("Segment size"),wxPG_LABEL,svs.gridSegmentSize);
			propGridSegmentSize->SetAttribute("Precision",svs.precision);
			AppendIn(propGrid,propGridSegmentSize);
		}

		propRenderHelpers = new BoolRefProperty(wxT("Helpers"), svs.renderHelpers, wxT("Helpers are non-scene elements rendered with scene, usually for diagnostic purposes."));
		AppendIn(propRenderExtras,propRenderHelpers);

		SetPropertyBackgroundColour(propRenderExtras,headerColor,false);
	}

	// GI
	{
		propGI = new wxStringProperty(wxT("GI"), wxPG_LABEL);
		Append(propGI);
		SetPropertyReadOnly(propGI,true,wxPG_DONT_RECURSE);

		propGIEmisMultiplier = new wxFloatProperty(wxT("Emissive multiplier"),wxPG_LABEL,svs.emissiveMultiplier);
		propGIEmisMultiplier->SetAttribute("Precision",svs.precision);
		AppendIn(propGI,propGIEmisMultiplier);

		propGIEmisVideoAffectsGI = new BoolRefProperty(wxT("Emissive video realtime GI"), svs.videoEmittanceAffectsGI);
		AppendIn(propGI,propGIEmisVideoAffectsGI);

		propGITranspVideoAffectsGI = new BoolRefProperty(wxT("Transparency video realtime GI"), svs.videoTransmittanceAffectsGI);
		AppendIn(propGI,propGITranspVideoAffectsGI);

		SetPropertyBackgroundColour(propGI,headerColor,false);
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
//! Must not be called in every frame, float property that is unhid in every frame loses focus immediately after click, can't be edited.
void SVSceneProperties::updateHide()
{
	propToneMappingAutomatic->Hide(!svs.renderTonemapping,false);
	propToneMappingAutomaticMin->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingAutomaticMax->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingAutomaticTarget->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingAutomaticSpeed->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingBrightness->Hide(!svs.renderTonemapping,false);
	propToneMappingContrast->Hide(!svs.renderTonemapping,false);
	propWaterColor->Hide(!svs.renderWater,false);
	propWaterLevel->Hide(!svs.renderWater,false);
	propGridNumSegments->Hide(!svs.renderGrid,false);
	propGridSegmentSize->Hide(!svs.renderGrid,false);
}

void SVSceneProperties::updateProperties()
{
	unsigned numChangesRelevantForHiding =
		+ updateBoolRef(propToneMapping)
		+ updateBool(propToneMappingAutomatic,svs.tonemappingAutomatic)
		+ updateBoolRef(propWater)
		+ updateBoolRef(propGrid)
		;
	unsigned numChangesOther =
		+ updateFloat(propCameraSpeed,svs.cameraMetersPerSecond)
		+ updateProperty(propCameraPosition,svs.eye.pos)
		+ updateProperty(propCameraAngles,RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle))
		+ updateFloat(propCameraFov,svs.eye.getFieldOfViewVerticalDeg())
		+ updateFloat(propCameraNear,svs.eye.getNear())
		+ updateFloat(propCameraFar,svs.eye.getFar())
		+ updateFloat(propToneMappingAutomaticMin,svs.tonemappingAutomaticMin)
		+ updateFloat(propToneMappingAutomaticMax,svs.tonemappingAutomaticMax)
		+ updateFloat(propToneMappingAutomaticTarget,svs.tonemappingAutomaticTarget)
		+ updateFloat(propToneMappingAutomaticSpeed,svs.tonemappingAutomaticSpeed)
		+ updateFloat(propToneMappingBrightness,svs.tonemappingBrightness[0])
		+ updateFloat(propToneMappingContrast,svs.tonemappingGamma)
		+ updateBoolRef(propRenderMaterialDiffuse)
		+ updateBoolRef(propRenderMaterialSpecular)
		+ updateBoolRef(propRenderMaterialEmittance)
		+ updateBoolRef(propRenderMaterialTransparency)
		+ updateBoolRef(propRenderMaterialTextures)
		+ updateBoolRef(propRenderWireframe)
		+ updateBoolRef(propRenderHelpers)
		+ updateBoolRef(propRenderFPS)
		+ updateBoolRef(propRenderLogo)
		+ updateBoolRef(propRenderVignettation)
		+ updateProperty(propWaterColor,svs.waterColor)
		+ updateFloat(propWaterLevel,svs.waterLevel)
		+ updateInt(propGridNumSegments,svs.gridNumSegments)
		+ updateFloat(propGridSegmentSize,svs.gridSegmentSize)
		+ updateFloat(propGIEmisMultiplier,svs.emissiveMultiplier)
		+ updateBoolRef(propGIEmisVideoAffectsGI)
		+ updateBoolRef(propGITranspVideoAffectsGI)
		;
	if (numChangesRelevantForHiding+numChangesOther)
	{
		Refresh();
		if (numChangesRelevantForHiding)
		{
			// Must not be called in every frame (e.g. because of auto tone mapping changing brightness in every frame),
			// float property that is unhid in every frame loses focus immediately after click, can't be edited.
			updateHide();
		}
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
		svs.eye.angle = property->GetPropertyByName("x")->GetValue().GetDouble();
		svs.eye.angleX = property->GetPropertyByName("y")->GetValue().GetDouble();
		svs.eye.leanAngle = property->GetPropertyByName("z")->GetValue().GetDouble();
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
		updateHide();
	}
	else
	if (property==propToneMappingAutomatic)
	{
		updateHide();
	}
	else
	if (property==propToneMappingAutomaticMin)
	{
		svs.tonemappingAutomaticMin = property->GetValue().GetDouble();
	}
	else
	if (property==propToneMappingAutomaticMax)
	{
		svs.tonemappingAutomaticMax = property->GetValue().GetDouble();
	}
	else
	if (property==propToneMappingAutomaticTarget)
	{
		svs.tonemappingAutomaticTarget = property->GetValue().GetDouble();
	}
	else
	if (property==propToneMappingAutomaticSpeed)
	{
		svs.tonemappingAutomaticSpeed = property->GetValue().GetDouble();
	}
	else
	if (property==propToneMappingBrightness)
	{
		svs.tonemappingBrightness = rr::RRVec4(property->GetValue().GetDouble());
	}
	if (property==propToneMappingContrast)
	{
		svs.tonemappingGamma = property->GetValue().GetDouble();
	}
	else
	if (property==propWater)
	{
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
	if (property==propGrid)
	{
		updateHide();
	}
	else
	if (property==propGridNumSegments)
	{
		svs.gridNumSegments = property->GetValue().GetInteger();
	}
	else
	if (property==propGridSegmentSize)
	{
		svs.gridSegmentSize = property->GetValue().GetDouble();
	}
	else
	if (property==propGIEmisMultiplier)
	{
		svs.emissiveMultiplier = property->GetValue().GetDouble();
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		frame->m_canvas->solver->setEmittance(svs.emissiveMultiplier,16,true);
	}
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
