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


		propCameraSpeed = new FloatProperty("Speed (m/s)",svs.cameraMetersPerSecond,svs.precision,0,1e10f,1,false);
		AppendIn(propCamera,propCameraSpeed);

		const wxChar* viewStrings[] = {wxT("Random (perspective)"),wxT("Top"),wxT("Bottom"),wxT("Front"),wxT("Back"),wxT("Left"),wxT("Right"),NULL};
		const long viewValues[] = {SVFrame::ME_VIEW_RANDOM,SVFrame::ME_VIEW_TOP,SVFrame::ME_VIEW_BOTTOM,SVFrame::ME_VIEW_FRONT,SVFrame::ME_VIEW_BACK,SVFrame::ME_VIEW_LEFT,SVFrame::ME_VIEW_RIGHT};
		propCameraView = new wxEnumProperty(wxT("View"), wxPG_LABEL, viewStrings, viewValues, SVFrame::ME_VIEW_RANDOM);
		AppendIn(propCamera,propCameraView);

		propCameraPosition = new RRVec3Property(wxT("Position"),wxPG_LABEL,svs.precision,svs.eye.pos,1);
		AppendIn(propCamera,propCameraPosition);

		propCameraDirection = new RRVec3Property(wxT("Direction"),wxPG_LABEL,svs.precision,svs.eye.dir,0.2f);
		AppendIn(propCamera,propCameraDirection);
		EnableProperty(propCameraDirection,false);

		propCameraAngles = new RRVec3Property(wxT("Angles"),wxPG_LABEL,svs.precision,RR_RAD2DEG(RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle)),10);
		AppendIn(propCamera,propCameraAngles);

		propCameraOrtho = new BoolRefProperty(wxT("Orthogonal"), svs.eye.orthogonal);
		AppendIn(propCamera,propCameraOrtho);

		propCameraOrthoSize = new FloatProperty(wxT("Size"), svs.eye.orthoSize,svs.precision,0,1000000,10,false);
		AppendIn(propCameraOrtho,propCameraOrthoSize);

		propCameraFov = new FloatProperty("FOV vertical degrees",svs.eye.getFieldOfViewVerticalDeg(),svs.precision,0,180,10,false);
		AppendIn(propCameraOrtho,propCameraFov);

		propCameraNear = new FloatProperty("Near",svs.eye.getNear(),svs.precision,0,1e10f,0.1f,false);
		AppendIn(propCamera,propCameraNear);
		EnableProperty(propCameraNear,false);

		propCameraFar = new FloatProperty("Far",svs.eye.getFar(),svs.precision,0,1e10f,1,false);
		AppendIn(propCamera,propCameraFar);
		EnableProperty(propCameraFar,false);

		propCameraCenter = new RRVec2Property(wxT("Center of screen"),wxPG_LABEL,svs.precision,svs.eye.screenCenter,1);
		AppendIn(propCamera,propCameraCenter);

		SetPropertyBackgroundColour(propCamera,headerColor,false);
	}

	// tone mapping
	{
		propToneMapping = new BoolRefProperty(wxT("Tone Mapping"), svs.renderTonemapping);
		Append(propToneMapping);

		propToneMappingAutomatic = new BoolRefProperty(wxT("Automatic"), svs.tonemappingAutomatic);
		AppendIn(propToneMapping,propToneMappingAutomatic);

		{
			propToneMappingAutomaticTarget = new FloatProperty("Target screen intensity",svs.tonemappingAutomaticTarget,svs.precision,0.1f,0.9f,0.1f,false);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticTarget);

			propToneMappingAutomaticSpeed = new FloatProperty("Speed",svs.tonemappingAutomaticSpeed,svs.precision,0,100,1,false);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticSpeed);
		}

		propToneMappingBrightness = new FloatProperty("Brightness",svs.tonemappingBrightness[0],svs.precision,0,1000,0.1f,false);
		AppendIn(propToneMapping,propToneMappingBrightness);

		propToneMappingContrast = new FloatProperty("Contrast",svs.tonemappingGamma,svs.precision,0,100,0.1f,false);
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

			propWaterLevel = new FloatProperty("Level",svs.waterLevel,svs.precision,-1e10f,1e10f,1,false);
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

			propGridNumSegments = new FloatProperty("Segments",svs.gridNumSegments,svs.precision,1,1000,10,false);
			AppendIn(propGrid,propGridNumSegments);

			propGridSegmentSize = new FloatProperty("Segment size",svs.gridSegmentSize,svs.precision,0,1e10f,1,false);
			AppendIn(propGrid,propGridSegmentSize);
		}

		propRenderHelpers = new BoolRefProperty(wxT("Helpers"), svs.renderHelpers, wxT("Helpers are non-scene elements rendered with scene, usually for diagnostic purposes."));
		AppendIn(propRenderExtras,propRenderHelpers);

		SetPropertyBackgroundColour(propRenderExtras,headerColor,false);
	}

	// GI
	{
		wxPGProperty* propGI = new wxStringProperty(wxT("GI"), wxPG_LABEL);
		Append(propGI);
		SetPropertyReadOnly(propGI,true,wxPG_DONT_RECURSE);

		propGIEmisMultiplier = new FloatProperty("Emissive multiplier",svs.emissiveMultiplier,svs.precision,0,1e10f,1,false);
		AppendIn(propGI,propGIEmisMultiplier);

		propGIEmisVideoAffectsGI = new BoolRefProperty(wxT("Emissive video realtime GI"), svs.videoEmittanceAffectsGI);
		AppendIn(propGI,propGIEmisVideoAffectsGI);

		propGITranspVideoAffectsGI = new BoolRefProperty(wxT("Transparency video realtime GI"), svs.videoTransmittanceAffectsGI);
		AppendIn(propGI,propGITranspVideoAffectsGI);

		SetPropertyBackgroundColour(propGI,headerColor,false);
	}

	updateHide();
}

//! Copy svs -> hide/show property.
//! Must not be called in every frame, float property that is unhid in every frame loses focus immediately after click, can't be edited.
void SVSceneProperties::updateHide()
{
	propCameraFov->Hide(svs.eye.orthogonal,false);
	propCameraOrthoSize->Hide(!svs.eye.orthogonal,false);
	propToneMappingAutomatic->Hide(!svs.renderTonemapping,false);
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
		+ updateBoolRef(propCameraOrtho)
		+ updateBoolRef(propToneMapping)
		+ updateBool(propToneMappingAutomatic,svs.tonemappingAutomatic)
		+ updateBoolRef(propWater)
		+ updateBoolRef(propGrid)
		;
	unsigned numChangesOther =
		+ updateFloat(propCameraSpeed,svs.cameraMetersPerSecond)
		+ updateProperty(propCameraPosition,svs.eye.pos)
		+ updateProperty(propCameraAngles,RR_RAD2DEG(RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle)))
		+ updateFloat(propCameraOrthoSize,svs.eye.orthoSize)
		+ updateFloat(propCameraFov,svs.eye.getFieldOfViewVerticalDeg())
		+ updateFloat(propCameraNear,svs.eye.getNear())
		+ updateFloat(propCameraFar,svs.eye.getFar())
		+ updateProperty(propCameraCenter,svs.eye.screenCenter)
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

	if (property==propCameraView)
	{
		int menuCode = property->GetValue().GetInteger();
		// our parent must be frame
		SVFrame* frame = (SVFrame*)GetParent();
		frame->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,menuCode));
	}
	else
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
		svs.eye.angle = RR_DEG2RAD(property->GetPropertyByName("x")->GetValue().GetDouble());
		svs.eye.angleX = RR_DEG2RAD(property->GetPropertyByName("y")->GetValue().GetDouble());
		svs.eye.leanAngle = RR_DEG2RAD(property->GetPropertyByName("z")->GetValue().GetDouble());
	}
	else
	if (property==propCameraOrtho)
	{
		updateHide();
	}
	else
	if (property==propCameraOrthoSize)
	{
		svs.eye.orthoSize = property->GetValue().GetDouble();
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
	if (property==propCameraCenter)
	{
		svs.eye.screenCenter << property->GetValue();
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
