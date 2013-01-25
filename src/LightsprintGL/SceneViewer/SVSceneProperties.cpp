// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneProperties.h"
#include "../Workaround.h"
#include "wx/datetime.h" // Tm

namespace rr_gl
{

int view2ME_VIEW(rr::RRCamera::View view)
{
	switch (view)
	{
		case rr::RRCamera::TOP: return SVFrame::ME_VIEW_TOP;
		case rr::RRCamera::BOTTOM: return SVFrame::ME_VIEW_BOTTOM;
		case rr::RRCamera::FRONT: return SVFrame::ME_VIEW_FRONT;
		case rr::RRCamera::BACK: return SVFrame::ME_VIEW_BACK;
		case rr::RRCamera::LEFT: return SVFrame::ME_VIEW_LEFT;
		case rr::RRCamera::RIGHT: return SVFrame::ME_VIEW_RIGHT;
	};
	return SVFrame::ME_VIEW_RANDOM;
}

SVSceneProperties::SVSceneProperties(SVFrame* _svframe)
	: SVProperties(_svframe)
{
	// camera
	{
		propCamera = new wxStringProperty(_("Camera"), wxPG_LABEL);
		Append(propCamera);
		SetPropertyReadOnly(propCamera,true,wxPG_DONT_RECURSE);

		// stereo
		{
			propCameraStereo = new BoolRefProperty(_("Stereo"),_("Enables stereo rendering. See User preferences/Stereo for stereo modes and options."),svs.renderStereo);
			AppendIn(propCamera,propCameraStereo);

			propCameraEyeSeparation = new FloatProperty(_("Eye separation")+" (m)",_("Distance from left to right eye. Negative value swaps left and right eye."),svs.eye.eyeSeparation,svs.precision,-1000,1000,0.01f,false);
			AppendIn(propCameraStereo,propCameraEyeSeparation);

			propCameraFocalLength = new FloatProperty(_("Focal length")+" (m)",_("How distant objects should appear in display plane."),svs.eye.focalLength,svs.precision,0,1e10,1,false);
			AppendIn(propCameraStereo,propCameraFocalLength);

		}


		propCameraSpeed = new FloatProperty(_("Speed")+" (m/s)",_("Controls how quickly camera moves when controlled by arrows/wsad."),svs.cameraMetersPerSecond,svs.precision,0,1e10f,1,false);
		AppendIn(propCamera,propCameraSpeed);

		const wxChar* viewStrings[] = {_("Custom"),_("Top"),_("Bottom"),_("Front"),_("Back"),_("Left"),_("Right"),NULL};
		const long viewValues[] = {SVFrame::ME_VIEW_RANDOM,SVFrame::ME_VIEW_TOP,SVFrame::ME_VIEW_BOTTOM,SVFrame::ME_VIEW_FRONT,SVFrame::ME_VIEW_BACK,SVFrame::ME_VIEW_LEFT,SVFrame::ME_VIEW_RIGHT};
		propCameraView = new wxEnumProperty(_("View"), wxPG_LABEL, viewStrings, viewValues);
		AppendIn(propCamera,propCameraView);

		propCameraPosition = new RRVec3Property(_("Position")+" (m)",_("Camera position in world space"),svs.precision,svs.eye.getPosition(),1);
		AppendIn(propCamera,propCameraPosition);

		propCameraDirection = new RRVec3Property(_("Direction"),_("Camera direction in world space, normalized"),svs.precision,svs.eye.getDirection(),0.2f);
		AppendIn(propCamera,propCameraDirection);
		EnableProperty(propCameraDirection,false);

		propCameraAngles = new RRVec3Property(_("Angles")+L" (\u00b0)",_("Camera direction in angles: yaw, pitch, roll (or azimuth, elevation, leaning)"),svs.precision,RR_RAD2DEG(svs.eye.getYawPitchRollRad()),10);
		AppendIn(propCamera,propCameraAngles);

		propCameraOrtho = new wxBoolProperty(_("Orthogonal"));
		propCameraOrtho->SetHelpString(_("Switches between orthogonal and perspective camera."));
		SetPropertyEditor(propCameraOrtho,wxPGEditor_CheckBox);
		AppendIn(propCamera,propCameraOrtho);

		propCameraOrthoSize = new FloatProperty(_("Size")+" (m)",_("World space distance between top and bottom of viewport."),svs.eye.getOrthoSize(),svs.precision,0,1000000,10,false);
		AppendIn(propCameraOrtho,propCameraOrthoSize);

		propCameraFov = new FloatProperty(_("FOV vertical")+L" (\u00b0)",_("Vertical field of view angle, angle between top and bottom of viewport"),svs.eye.getFieldOfViewVerticalDeg(),svs.precision,0,180,10,false);
		AppendIn(propCamera,propCameraFov);
		// why it's not safe to move FOV above Ortho:
		//   when Ortho is checked, FOV disappears and Ortho moves one line up, but just checked Ortho checkbox is rendered into old position

		propCameraNear = new FloatProperty(_("Near")+" (m)",_("Near plane distance, elements closer to camera are not rendered."),svs.eye.getNear(),svs.precision,-1e10f,1e10f,0.1f,false);
		AppendIn(propCamera,propCameraNear);

		propCameraFar = new FloatProperty(_("Far")+" (m)",_("Far plane distance, elements farther from camera are not rendered."),svs.eye.getFar(),svs.precision,-1e10f,1e10f,1,false);
		AppendIn(propCamera,propCameraFar);

		propCameraRangeAutomatic = new BoolRefProperty(_("Automatic near/far"),_("Near/far is set automatically based on distance of objects in viewport."),svs.cameraDynamicNear);
		AppendIn(propCamera,propCameraRangeAutomatic);

		propCameraCenter = new RRVec2Property(_("Center of screen"),_("Shifts look up/down/left/right without distorting image. E.g. in architecture, 0,0.3 moves horizon down without skewing vertical lines."),svs.precision,svs.eye.getScreenCenter(),1);
		AppendIn(propCamera,propCameraCenter);

		SetPropertyBackgroundColour(propCamera,importantPropertyBackgroundColor,false);
	}

	// environment
	{
		propEnv = new wxStringProperty(_("Environment"), wxPG_LABEL);
		Append(propEnv);
		SetPropertyReadOnly(propEnv,true,wxPG_DONT_RECURSE);

		propEnvMap = new ImageFileProperty(_("Sky texture or video"),_("Supported formats: equirectangular panoramas, cross-shaped 3:4 and 4:3 images, Quake-like sets of 6 images, 40+ fileformats including HDR. Type in c@pture to use live video input as environment."));
		// string is updated from OnIdle
		AppendIn(propEnv,propEnvMap);

		propEnvMapAngleDeg = new FloatProperty(_("Rotation")+L" (\u00b0)",_("Rotates environment around up-down axis by this number of degrees."),svs.skyboxRotationRad,svs.precision,0,360,10,true);
		AppendIn(propEnvMap,propEnvMapAngleDeg);

		//propEnvSimulateSky = new BoolRefProperty(_("Simulate sky"),_("Work in progress, has no effect."),svs.envSimulateSky);
		//AppendIn(propEnv,propEnvSimulateSky);

		propEnvSimulateSun = new BoolRefProperty(_("Simulate Sun"),_("Calculates Sun position from date, time and location. Affects first directional light only, insert one if none exists. World directions: north=Z+, east=X+, up=Y+."),svs.envSimulateSun);
		AppendIn(propEnv,propEnvSimulateSun);

		propEnvLocation = new LocationProperty(_("Location"),_("Geolocation used for Sun and sky simulation."),svs.precision,rr::RRVec2(svs.envLatitudeDeg,svs.envLongitudeDeg));
		AppendIn(propEnv,propEnvLocation);

		propEnvDate = new wxDateProperty(_("Date"),wxPG_LABEL,wxDateTime(svs.envDateTime));
		propEnvDate->SetHelpString(_("Date used for Sun and sky simulation."));
		AppendIn(propEnv,propEnvDate);

		// hours will use wxTimePickerCtrl when wx2.9.3 is released
		propEnvTime = new FloatProperty(_("Local time (hour)"),_("Hour, 0..24, local time used for Sun and sky simulation."),svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.f,svs.precision,0,24,1,true);
		AppendIn(propEnv,propEnvTime);

		propEnvSpeed = new FloatProperty(_("Speed"),_("How quickly simulation runs, 0=stopped, 1=realtime, 3600=one day in 24seconds."),svs.envSpeed,svs.precision,0,1000000,1,false);
		AppendIn(propEnv,propEnvSpeed);

		SetPropertyBackgroundColour(propEnv,importantPropertyBackgroundColor,false);
	}

	// tone mapping
	{
		propToneMapping = new BoolRefProperty(_("Tone Mapping"),_("Enables fullscreen brightness and contrast adjustments."),svs.renderTonemapping);
		Append(propToneMapping);

		propToneMappingAutomatic = new BoolRefProperty(_("Automatic"),_("Makes brightness adjust automatically."),svs.tonemappingAutomatic);
		AppendIn(propToneMapping,propToneMappingAutomatic);

		{
			propToneMappingAutomaticTarget = new FloatProperty(_("Target screen intensity"),_("Automatic tone mapping tries to reach this average image intensity."),svs.tonemappingAutomaticTarget,svs.precision,0.125f,0.875f,0.1f,false);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticTarget);

			propToneMappingAutomaticSpeed = new FloatProperty(_("Speed"),_("Speed of automatic tone mapping changes."),svs.tonemappingAutomaticSpeed,svs.precision,0,100,1,false);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticSpeed);
		}

		propToneMappingBrightness = new FloatProperty(_("Brightness"),_("Brightness correction applied to rendered images, default=1."),svs.tonemappingBrightness[0],svs.precision,0,1000,0.1f,false);
		AppendIn(propToneMapping,propToneMappingBrightness);

		propToneMappingContrast = new FloatProperty(_("Contrast"),_("Contrast correction applied to rendered images, default=1."),svs.tonemappingGamma,svs.precision,0,100,0.1f,false);
		AppendIn(propToneMapping,propToneMappingContrast);

		SetPropertyBackgroundColour(propToneMapping,importantPropertyBackgroundColor,false);
	}

	// render materials
	{
		propRenderMaterials = new wxStringProperty(_("Render materials"), wxPG_LABEL);
		Append(propRenderMaterials);
		SetPropertyReadOnly(propRenderMaterials,true,wxPG_DONT_RECURSE);

		propRenderMaterialDiffuse = new BoolRefProperty(_("Diffuse color"),_("Toggles between rendering diffuse colors and diffuse white. With diffuse color disabled, color bleeding is usually clearly visible."),svs.renderMaterialDiffuse);
		AppendIn(propRenderMaterials,propRenderMaterialDiffuse);

		propRenderMaterialSpecular = new BoolRefProperty(_("Specular"),_("Toggles rendering specular reflections. Disabling them could make huge highly specular scenes render faster."),svs.renderMaterialSpecular);
		AppendIn(propRenderMaterials,propRenderMaterialSpecular);

		propRenderMaterialEmittance = new BoolRefProperty(_("Emittance"),_("Toggles rendering emittance of emissive surfaces."),svs.renderMaterialEmission);
		AppendIn(propRenderMaterials,propRenderMaterialEmittance);

		const wxChar* tfrStrings[] = {_("off (opaque)"),_("1-bit (alpha keying)"),_("8-bit (alpha blending)"),_("24bit (RGB blending)"),NULL};
		const long tfrValues[] = {T_OPAQUE,T_ALPHA_KEY,T_ALPHA_BLEND,T_RGB_BLEND};
		propRenderMaterialTransparency = new wxEnumProperty(_("Transparency"),wxPG_LABEL,tfrStrings,tfrValues);
		propRenderMaterialTransparency->SetHelpString(_("Changes how realistically semi-transparent surfaces are rendered."));
		AppendIn(propRenderMaterials,propRenderMaterialTransparency);

		propRenderMaterialTransparencyFresnel = new BoolRefProperty(_("Fresnel"),_("Toggles using fresnel term."),svs.renderMaterialTransparencyFresnel);
		AppendIn(propRenderMaterialTransparency,propRenderMaterialTransparencyFresnel);

		propRenderMaterialBumpMaps = new BoolRefProperty(_("Bump maps"),_("Toggles rendering bump maps."),svs.renderMaterialBumpMaps);
		AppendIn(propRenderMaterials,propRenderMaterialBumpMaps);

		propRenderMaterialTextures = new BoolRefProperty(_("Textures"),_("Toggles between material textures and flat colors. Disabling textures could make rendering faster.")+" (ctrl-t)",svs.renderMaterialTextures);
		AppendIn(propRenderMaterials,propRenderMaterialTextures);

		propRenderMaterialSidedness = new BoolRefProperty(_("Sidedness"),_("Check to honour front/back visibility flags in material, uncheck to render all faces as 2-sided."),svs.renderMaterialSidedness);
		AppendIn(propRenderMaterials,propRenderMaterialSidedness);

		SetPropertyBackgroundColour(propRenderMaterials,importantPropertyBackgroundColor,false);
	}

	// render extras
	{
		propRenderExtras = new wxStringProperty(_("Render extras"), wxPG_LABEL);
		Append(propRenderExtras);
		SetPropertyReadOnly(propRenderExtras,true,wxPG_DONT_RECURSE);

		propRenderWireframe = new BoolRefProperty(_("Wireframe"),_("Toggles between solid and wireframe rendering modes.")+" (ctrl-w)",svs.renderWireframe);
		AppendIn(propRenderExtras,propRenderWireframe);

		propRenderFPS = new BoolRefProperty(_("FPS"),_("FPS counter shows number of frames rendered in last second.")+" (ctrl-f)",svs.renderFPS);
		AppendIn(propRenderExtras,propRenderFPS);

		// logo
		{
			propLogo = new BoolRefProperty(_("Logo"),_("Logo is loaded from data/maps/sv_logo.png."),svs.renderLogo);
			AppendIn(propRenderExtras,propLogo);
		}

		propRenderBloom = new BoolRefProperty(_("Bloom"),_("Applies fullscreen bloom effect."),svs.renderBloom);
		AppendIn(propRenderExtras,propRenderBloom);

		propRenderDOF = new BoolRefProperty(_("DOF"),_("Applies fullscreen depth of field effect. Focus is controlled by camera's focal length."),svs.renderDOF);
		AppendIn(propRenderExtras,propRenderDOF);

		// lens flare
		{
			propLensFlare = new BoolRefProperty(_("Lens Flare"),_("Renders lens flare for all directional lights."),svs.renderLensFlare);
			AppendIn(propRenderExtras,propLensFlare);

			propLensFlareSize = new FloatProperty(_("Size"),_("Relative size of lens flare effects."),(float)svs.lensFlareSize,svs.precision,0,40,0.5,false);
			AppendIn(propLensFlare,propLensFlareSize);

			propLensFlareId = new FloatProperty(_("Id"),_("Other lens flare parameters are generated from this number; change it randomly until you get desired look."),(float)svs.lensFlareId,svs.precision,1,(float)UINT_MAX,10,true);
			AppendIn(propLensFlare,propLensFlareId);
		}

		// vignette
		{
			propVignette = new BoolRefProperty(_("Vignette"),_("Renders vignette overlay image."),svs.renderVignette);
			AppendIn(propRenderExtras,propVignette);

		}

		// grid
		{
			propGrid = new BoolRefProperty(_("Grid"),_("Toggles rendering 2d grid in y=0 plane, around world center."),svs.renderGrid);
			AppendIn(propRenderExtras,propGrid);

			propGridNumSegments = new FloatProperty(_("Segments"),_("Number of grid segments per line, e.g. 10 makes grid of 10x10 squares."),svs.gridNumSegments,svs.precision,1,1000,10,false);
			AppendIn(propGrid,propGridNumSegments);

			propGridSegmentSize = new FloatProperty(_("Segment size")+" (m)",_("Distance between grid lines."),svs.gridSegmentSize,svs.precision,0,1e10f,1,false);
			AppendIn(propGrid,propGridSegmentSize);
		}

		propRenderHelpers = new BoolRefProperty(_("Helpers"),_("Helpers are non-scene elements rendered with scene, usually for diagnostic purposes."),svs.renderHelpers);
		AppendIn(propRenderExtras,propRenderHelpers);

		SetPropertyBackgroundColour(propRenderExtras,importantPropertyBackgroundColor,false);
	}

	updateHide();
}

//! Copy svs -> hide/show property.
//! Must not be called in every frame, float property that is unhid in every frame loses focus immediately after click, can't be edited.
void SVSceneProperties::updateHide()
{
	propCameraEyeSeparation->Hide(!svs.renderStereo,false);
	propCameraFocalLength->Hide(!svs.renderStereo,false);

	propCameraFov->Hide(svs.eye.isOrthogonal(),false);
	propCameraOrthoSize->Hide(!svs.eye.isOrthogonal(),false);
	EnableProperty(propCameraNear,!svs.cameraDynamicNear);
	EnableProperty(propCameraFar,!svs.cameraDynamicNear);

	propEnvMap->Hide(svs.envSimulateSky,false);
	propEnvMapAngleDeg->Hide(svs.envSimulateSky||!svframe->m_canvas||!svframe->m_canvas->solver||!svframe->m_canvas->solver->getEnvironment(),false);
	propEnvLocation->Hide(!svs.envSimulateSky&&!svs.envSimulateSun);
	propEnvDate->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);
	propEnvTime->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);
	propEnvSpeed->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);

	propToneMappingAutomatic->Hide(!svs.renderTonemapping,false);
	propToneMappingAutomaticTarget->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingAutomaticSpeed->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingBrightness->Hide(!svs.renderTonemapping,false);
	propToneMappingContrast->Hide(!svs.renderTonemapping,false);

	propRenderMaterialTransparencyFresnel->Hide(svs.renderMaterialTransparency==T_OPAQUE || svs.renderMaterialTransparency==T_ALPHA_KEY,false);


	propLensFlareSize->Hide(!svs.renderLensFlare,false);
	propLensFlareId->Hide(!svs.renderLensFlare,false);


	propGridNumSegments->Hide(!svs.renderGrid,false);
	propGridSegmentSize->Hide(!svs.renderGrid,false);

}

void SVSceneProperties::updateProperties()
{
	// This function can update any property that has suddenly changed in svs, outside property grid.
	// We depend on it when new svs is loaded from disk.
	// Other approach would be to create all properties again, however,
	// this function would still have to support at least properties that user can change by hotkeys or mouse navigation.
	unsigned numChangesRelevantForHiding =
		+ updateBoolRef(propCameraStereo)
		+ updateBool(propCameraOrtho,svs.eye.isOrthogonal())
		+ updateBoolRef(propCameraRangeAutomatic)
		//+ updateBoolRef(propEnvSimulateSky)
		+ updateString(propEnvMap,(svframe->m_canvas&&svframe->m_canvas->solver&&svframe->m_canvas->solver->getEnvironment())?RR_RR2WX(svframe->m_canvas->solver->getEnvironment()->filename):L"(no texture)")
		+ updateBoolRef(propEnvSimulateSun)
		+ updateBoolRef(propToneMapping)
		+ updateBool(propToneMappingAutomatic,svs.tonemappingAutomatic)
		+ updateBoolRef(propLogo)
		+ updateBoolRef(propLensFlare)
		+ updateBoolRef(propVignette)
		+ updateBoolRef(propGrid)
		;

	unsigned numChangesOther =
		+ updateFloat(propCameraEyeSeparation,svs.eye.eyeSeparation)
		+ updateFloat(propCameraFocalLength,svs.eye.focalLength)
		+ updateFloat(propCameraSpeed,svs.cameraMetersPerSecond)
		+ updateProperty(propCameraPosition,svs.eye.getPosition())
		+ updateProperty(propCameraDirection,svs.eye.getDirection())
		+ updateProperty(propCameraAngles,RR_RAD2DEG(svs.eye.getYawPitchRollRad()))
		+ updateInt(propCameraView,view2ME_VIEW(svs.eye.getView()))
		+ updateFloat(propCameraOrthoSize,svs.eye.getOrthoSize())
		+ updateFloat(propCameraFov,svs.eye.getFieldOfViewVerticalDeg())
		+ updateFloat(propCameraNear,svs.eye.getNear())
		+ updateFloat(propCameraFar,svs.eye.getFar())
		+ updateProperty(propCameraCenter,svs.eye.getScreenCenter())
		//+ updateBoolRef(propEnvSimulateSky)
		+ updateBoolRef(propEnvSimulateSun)
		+ updateFloat(propEnvMapAngleDeg,RR_RAD2DEG(svs.skyboxRotationRad))
		+ updateProperty(propEnvLocation,rr::RRVec2(svs.envLatitudeDeg,svs.envLongitudeDeg))
		+ updateDate(propEnvDate,wxDateTime(svs.envDateTime))
		+ updateFloat(propEnvTime,svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.f+svs.envDateTime.tm_sec/3600.f)
		+ updateFloat(propEnvSpeed,svs.envSpeed)
		+ updateFloat(propToneMappingAutomaticTarget,svs.tonemappingAutomaticTarget)
		+ updateFloat(propToneMappingAutomaticSpeed,svs.tonemappingAutomaticSpeed)
		+ updateFloat(propToneMappingBrightness,svs.tonemappingBrightness[0])
		+ updateFloat(propToneMappingContrast,svs.tonemappingGamma)
		+ updateBoolRef(propRenderMaterialDiffuse)
		+ updateBoolRef(propRenderMaterialSpecular)
		+ updateBoolRef(propRenderMaterialEmittance)
		+ updateInt(propRenderMaterialTransparency,svs.renderMaterialTransparency)
		+ updateBoolRef(propRenderMaterialTransparencyFresnel)
		+ updateBoolRef(propRenderMaterialBumpMaps)
		+ updateBoolRef(propRenderMaterialTextures)
		+ updateBoolRef(propRenderMaterialSidedness)
		+ updateBoolRef(propRenderWireframe)
		+ updateBoolRef(propRenderHelpers)
		+ updateBoolRef(propRenderFPS)
		+ updateBoolRef(propRenderBloom)
		+ updateBoolRef(propRenderDOF)
		+ updateFloat(propLensFlareSize,svs.lensFlareSize)
		+ updateFloat(propLensFlareId,svs.lensFlareId)
		+ updateInt(propGridNumSegments,svs.gridNumSegments)
		+ updateFloat(propGridSegmentSize,svs.gridSegmentSize)
		;
	if (numChangesRelevantForHiding+numChangesOther)
	{
		Refresh();
		//rr::RRReporter::report(rr::INF2,"Scene props refresh\n");
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
	if (IS_SHOWN(this))
	{
		updateProperties();
	}
}

void SVSceneProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propCameraStereo)
	{
		updateHide();
	}
	else
	if (property==propCameraEyeSeparation)
	{
		svs.eye.eyeSeparation = property->GetValue().GetDouble();
	}
	else
	if (property==propCameraFocalLength)
	{
		svs.eye.focalLength = property->GetValue().GetDouble();
	}
	else
	if (property==propCameraView)
	{
		int menuCode = property->GetValue().GetInteger();
		svframe->OnMenuEventCore(menuCode);
	}
	else
	if (property==propCameraSpeed)
	{
		svs.cameraMetersPerSecond = property->GetValue().GetDouble();
	}
	else
	if (property==propCameraPosition)
	{
		RRVec3 pos;
		pos << property->GetValue();
		svs.eye.setPosition(pos);
	}
	else
	if (property==propCameraAngles)
	{
		RRVec3 yawPitchRollRad;
		yawPitchRollRad << property->GetValue();
		svs.eye.setYawPitchRollRad(RR_DEG2RAD(yawPitchRollRad));
	}
	else
	if (property==propCameraOrtho)
	{
		svs.eye.setOrthogonal(property->GetValue().GetBool());
		updateHide();
	}
	else
	if (property==propCameraOrthoSize)
	{
		svs.eye.setOrthoSize(property->GetValue().GetDouble());
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
	if (property==propCameraRangeAutomatic)
	{
		updateHide();
	}
	else
	if (property==propCameraCenter)
	{
		RRVec2 tmp;
		tmp << property->GetValue();
		svs.eye.setScreenCenter(tmp);
	}
	else
	//if (property==propEnvSimulateSky)
	//{
	//	updateHide();
	//}
	//else
	if (property==propEnvSimulateSun)
	{
		updateHide();
		svframe->simulateSun();
	}
	else
	if (property==propEnvMap)
	{
		svs.skyboxFilename = RR_WX2RR(propEnvMap->GetValue().GetString());
		svframe->OnMenuEventCore(SVFrame::ME_ENV_RELOAD);
		updateHide();
	}
	else
	if (property==propEnvMapAngleDeg)
	{
		svs.skyboxRotationRad = RR_DEG2RAD(propEnvMapAngleDeg->GetValue().GetDouble());
		svframe->m_canvas->solver->setEnvironment(svframe->m_canvas->solver->getEnvironment(0),svframe->m_canvas->solver->getEnvironment(1),svs.skyboxRotationRad,svs.skyboxRotationRad);
	}
	else
	if (property==propEnvLocation)
	{
		rr::RRVec2 latitudeLongitude;
		latitudeLongitude << property->GetValue();
		svs.envLatitudeDeg = latitudeLongitude[0];
		svs.envLongitudeDeg = latitudeLongitude[1];
		svframe->simulateSun();
	}
	else
	if (property==propEnvDate)
	{
		wxDateTime::Tm t = property->GetValue().GetDateTime().GetTm();
		//svs.envDateTime.tm_isdst =
		svs.envDateTime.tm_mday = t.mday;
		svs.envDateTime.tm_mon = t.mon;
		svs.envDateTime.tm_wday = t.GetWeekDay();
		//svs.envDateTime.tm_yday = t.yday;
		svs.envDateTime.tm_year = t.year-1900;
		svframe->simulateSun();
	}
	else
	if (property==propEnvTime)
	{
		float h = property->GetValue().GetDouble();
		svs.envDateTime.tm_hour = (int)h;
		svs.envDateTime.tm_min = fmodf(h,1)*60;
		svs.envDateTime.tm_sec = 0;
		svs.envDateTime.tm_nsec = 0;
		svframe->simulateSun();
	}
	else
	if (property==propEnvSpeed)
	{
		svs.envSpeed = property->GetValue().GetDouble();
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
	else
	if (property==propToneMappingContrast)
	{
		svs.tonemappingGamma = property->GetValue().GetDouble();
	}
	else
	if (property==propRenderMaterialTransparency)
	{
		svs.renderMaterialTransparency = (Transparency)(property->GetValue().GetInteger());
		updateHide();
	}
	else
	if (property==propRenderMaterialTransparencyFresnel)
	{
		// update rgb shadowmaps
		svframe->m_canvas->solver->reportDirectIlluminationChange(-1,true,false,false);
	}
	else
	if (property==propRenderMaterialSidedness)
	{
		// update shadowmaps
		svframe->m_canvas->solver->reportDirectIlluminationChange(-1,true,false,false);
	}
	else
	if (property==propLensFlare)
	{
		updateHide();
	}
	else
	if (property==propLensFlareSize)
	{
		svs.lensFlareSize = property->GetValue().GetDouble();
	}
	else
	if (property==propLensFlareId)
	{
		svs.lensFlareId = property->GetValue().GetInteger();
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
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
