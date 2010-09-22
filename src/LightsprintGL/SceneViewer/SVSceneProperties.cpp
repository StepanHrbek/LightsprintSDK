// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneProperties.h"
#include "SVFrame.h" // solver access
#include "SVCanvas.h" // solver access
#include "wx/datetime.h" // Tm

namespace rr_gl
{

int view2ME_VIEW(Camera::View view)
{
	switch (view)
	{
		case Camera::TOP: return SVFrame::ME_VIEW_TOP;
		case Camera::BOTTOM: return SVFrame::ME_VIEW_BOTTOM;
		case Camera::FRONT: return SVFrame::ME_VIEW_FRONT;
		case Camera::BACK: return SVFrame::ME_VIEW_BACK;
		case Camera::LEFT: return SVFrame::ME_VIEW_LEFT;
		case Camera::RIGHT: return SVFrame::ME_VIEW_RIGHT;
	};
	return SVFrame::ME_VIEW_RANDOM;
}

SVSceneProperties::SVSceneProperties(SVFrame* _svframe)
	: SVProperties(_svframe)
{
	wxColour headerColor(230,230,230);

	// camera
	{
		propCamera = new wxStringProperty(wxT("Camera"), wxPG_LABEL);
		Append(propCamera);
		SetPropertyReadOnly(propCamera,true,wxPG_DONT_RECURSE);


		propCameraSpeed = new FloatProperty("Speed (m/s)","Controls how quickly camera moves when controlled by arrows/wsad.",svs.cameraMetersPerSecond,svs.precision,0,1e10f,1,false);
		AppendIn(propCamera,propCameraSpeed);

		const wxChar* viewStrings[] = {wxT("Custom"),wxT("Top"),wxT("Bottom"),wxT("Front"),wxT("Back"),wxT("Left"),wxT("Right"),NULL};
		const long viewValues[] = {SVFrame::ME_VIEW_RANDOM,SVFrame::ME_VIEW_TOP,SVFrame::ME_VIEW_BOTTOM,SVFrame::ME_VIEW_FRONT,SVFrame::ME_VIEW_BACK,SVFrame::ME_VIEW_LEFT,SVFrame::ME_VIEW_RIGHT};
		propCameraView = new wxEnumProperty(wxT("View"), wxPG_LABEL, viewStrings, viewValues);
		AppendIn(propCamera,propCameraView);

		propCameraPosition = new RRVec3Property(wxT("Position (m)"),"Camera position in world space",svs.precision,svs.eye.pos,1);
		AppendIn(propCamera,propCameraPosition);

		propCameraDirection = new RRVec3Property(wxT("Direction"),"Camera direction in world space, normalized",svs.precision,svs.eye.dir,0.2f);
		AppendIn(propCamera,propCameraDirection);
		EnableProperty(propCameraDirection,false);

		propCameraAngles = new RRVec3Property(wxT("Angles (deg)"),"Camera direction in angles: azimuth, elevation, leaning",svs.precision,RR_RAD2DEG(RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle)),10);
		AppendIn(propCamera,propCameraAngles);

		propCameraOrtho = new BoolRefProperty(wxT("Orthogonal"),"Switches between orthogonal and perspective camera.",svs.eye.orthogonal);
		AppendIn(propCamera,propCameraOrtho);

		propCameraOrthoSize = new FloatProperty(wxT("Size (m)"),"World space distance between top and bottom of viewport.",svs.eye.orthoSize,svs.precision,0,1000000,10,false);
		AppendIn(propCameraOrtho,propCameraOrthoSize);

		propCameraFov = new FloatProperty("FOV vertical (deg)","Vertical field of view angle, angle between top and bottom of viewport",svs.eye.getFieldOfViewVerticalDeg(),svs.precision,0,180,10,false);
		AppendIn(propCameraOrtho,propCameraFov);

		propCameraNear = new FloatProperty("Near (m)","Near plane distance, elements closer to camera are not rendered.",svs.eye.getNear(),svs.precision,-1e10f,1e10f,0.1f,false);
		AppendIn(propCamera,propCameraNear);
		EnableProperty(propCameraNear,false);

		propCameraFar = new FloatProperty("Far (m)","Far plane distance, elements farther from camera are not rendered.",svs.eye.getFar(),svs.precision,-1e10f,1e10f,1,false);
		AppendIn(propCamera,propCameraFar);
		EnableProperty(propCameraFar,false);

		propCameraCenter = new RRVec2Property(wxT("Center of screen"),"Shifts look up/down/left/right without distorting image. E.g. in architecture, 0,0.3 moves horizon down without skewing vertical lines.",svs.precision,svs.eye.screenCenter,1);
		AppendIn(propCamera,propCameraCenter);

		SetPropertyBackgroundColour(propCamera,headerColor,false);
	}

	// environment
	{
		propEnv = new wxStringProperty(wxT("Environment"), wxPG_LABEL);
		Append(propEnv);
		SetPropertyReadOnly(propEnv,true,wxPG_DONT_RECURSE);

		propEnvMap = new ImageFileProperty(wxT("Skybox texture"),"Supported formats: cross-shaped 3:4 and 4:3 images, Quake-like sets of 6 images, 40+ fileformats including HDR.");
		// string is updated from OnIdle
		AppendIn(propEnv,propEnvMap);

		//propEnvSimulateSky = new BoolRefProperty(wxT("Simulate sky"),"Work in progress, has no effect.",svs.envSimulateSky);
		//AppendIn(propEnv,propEnvSimulateSky);

		propEnvSimulateSun = new BoolRefProperty(wxT("Simulate Sun"),"Calculates Sun position from date, time and location. Affects first directional light only, insert one if none exists. World directions: north=Z+, east=X+, up=Y+.",svs.envSimulateSun);
		AppendIn(propEnv,propEnvSimulateSun);

		propEnvLocation = new LocationProperty(wxT("Location"),"Geolocation used for Sun and sky simulation.",svs.precision,rr::RRVec2(svs.envLatitudeDeg,svs.envLongitudeDeg));
		AppendIn(propEnv,propEnvLocation);

		propEnvDate = new wxDateProperty(wxT("Date"),wxPG_LABEL,wxDateTime(svs.envDateTime));
		propEnvDate->SetHelpString("Date used for Sun and sky simulation.");
		AppendIn(propEnv,propEnvDate);

		propEnvTime = new FloatProperty(wxT("Local time (hour)"),"Hour, 0..24, local time used for Sun and sky simulation.",svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.f,svs.precision,0,24,1,true);
		AppendIn(propEnv,propEnvTime);

		SetPropertyBackgroundColour(propEnv,headerColor,false);
	}

	// tone mapping
	{
		propToneMapping = new BoolRefProperty(wxT("Tone Mapping"),"Enables fullscreen brightness and contrast adjustments.",svs.renderTonemapping);
		Append(propToneMapping);

		propToneMappingAutomatic = new BoolRefProperty(wxT("Automatic"),"Makes brightness adjust automatically.",svs.tonemappingAutomatic);
		AppendIn(propToneMapping,propToneMappingAutomatic);

		{
			propToneMappingAutomaticTarget = new FloatProperty("Target screen intensity","Automatic tone mapping tries to reach this average image intensity.",svs.tonemappingAutomaticTarget,svs.precision,0.125f,0.875f,0.1f,false);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticTarget);

			propToneMappingAutomaticSpeed = new FloatProperty("Speed","Speed of automatic tone mapping changes.",svs.tonemappingAutomaticSpeed,svs.precision,0,100,1,false);
			AppendIn(propToneMappingAutomatic,propToneMappingAutomaticSpeed);
		}

		propToneMappingBrightness = new FloatProperty("Brightness","Brightness correction applied to rendered images, default=1.",svs.tonemappingBrightness[0],svs.precision,0,1000,0.1f,false);
		AppendIn(propToneMapping,propToneMappingBrightness);

		propToneMappingContrast = new FloatProperty("Contrast","Contrast correction applied to rendered images, default=1.",svs.tonemappingGamma,svs.precision,0,100,0.1f,false);
		AppendIn(propToneMapping,propToneMappingContrast);


		SetPropertyBackgroundColour(propToneMapping,headerColor,false);
	}

	// render materials
	{
		propRenderMaterials = new wxStringProperty(wxT("Render materials"), wxPG_LABEL);
		Append(propRenderMaterials);
		SetPropertyReadOnly(propRenderMaterials,true,wxPG_DONT_RECURSE);

		propRenderMaterialDiffuse = new BoolRefProperty("Diffuse color","Toggles between rendering diffuse colors and diffuse white. With diffuse color disabled, color bleeding is usually clearly visible.",svs.renderMaterialDiffuse);
		AppendIn(propRenderMaterials,propRenderMaterialDiffuse);

		propRenderMaterialSpecular = new BoolRefProperty("Specular","Toggles rendering specular reflections. Disabling them could make huge highly specular scenes render faster.",svs.renderMaterialSpecular);
		AppendIn(propRenderMaterials,propRenderMaterialSpecular);

		propRenderMaterialEmittance = new BoolRefProperty("Emittance","Toggles rendering emittance of emissive surfaces.",svs.renderMaterialEmission);
		AppendIn(propRenderMaterials,propRenderMaterialEmittance);

		propRenderMaterialTransparency = new BoolRefProperty("Transparency","Toggles rendering transparency of semi-transparent surfaces. Disabling it could make rendering faster.",svs.renderMaterialTransparency);
		AppendIn(propRenderMaterials,propRenderMaterialTransparency);

		propRenderMaterialTextures = new BoolRefProperty("Textures","(ctrl-t) Toggles between material textures and flat colors. Disabling textures could make rendering faster.",svs.renderMaterialTextures);
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
			propWater = new BoolRefProperty("Water","Enables rendering of water layer, with reflection and waves.",svs.renderWater);
			AppendIn(propRenderExtras,propWater);

			propWaterColor = new HDRColorProperty("Color","Color of scattered light coming out of water.",svs.precision,svs.waterColor);
			AppendIn(propWater,propWaterColor);

			propWaterLevel = new FloatProperty("Level","Altitude of water surface, Y coordinate in world space.",svs.waterLevel,svs.precision,-1e10f,1e10f,1,false);
			AppendIn(propWater,propWaterLevel);
		}

		propRenderWireframe = new BoolRefProperty("Wireframe","(ctrl-w) Toggles between solid and wireframe rendering modes.",svs.renderWireframe);
		AppendIn(propRenderExtras,propRenderWireframe);

		propRenderFPS = new BoolRefProperty("FPS","(ctrl-f) FPS counter shows number of frames rendered in last second.",svs.renderFPS);
		AppendIn(propRenderExtras,propRenderFPS);

		propRenderLogo = new BoolRefProperty("Logo","Logo is loaded from data/maps/sv_logo.png.",svs.renderLogo);
		AppendIn(propRenderExtras,propRenderLogo);

		propRenderBloom = new BoolRefProperty("Bloom","Applies fullscreen bloom effect.",svs.renderBloom);
		AppendIn(propRenderExtras,propRenderBloom);

		// lens flare
		{
			propLensFlare = new BoolRefProperty("Lens Flare","Renders lens flare for all directional lights.",svs.renderLensFlare);
			AppendIn(propRenderExtras,propLensFlare);

			propLensFlareSize = new FloatProperty("Size","Relative size of lens flare effects.",(float)svs.lensFlareSize,svs.precision,0,40,0.5,false);
			AppendIn(propLensFlare,propLensFlareSize);

			propLensFlareId = new FloatProperty("Id","Other lens flare parameters are generated from this number; change it randomly until you get desired look.",(float)svs.lensFlareId,svs.precision,1,(float)UINT_MAX,10,true);
			AppendIn(propLensFlare,propLensFlareId);
		}

		propRenderVignette = new BoolRefProperty("Vignette","Vignette overlay is loaded from data/maps/vignette.png.",svs.renderVignette);
		AppendIn(propRenderExtras,propRenderVignette);

		// grid
		{
			propGrid = new BoolRefProperty("Grid","Toggles rendering 2d grid in y=0 plane, around world center.",svs.renderGrid);
			AppendIn(propRenderExtras,propGrid);

			propGridNumSegments = new FloatProperty("Segments","Number of grid segments per line, e.g. 10 makes grid of 10x10 squares.",svs.gridNumSegments,svs.precision,1,1000,10,false);
			AppendIn(propGrid,propGridNumSegments);

			propGridSegmentSize = new FloatProperty("Segment size (m)","Distance between grid lines.",svs.gridSegmentSize,svs.precision,0,1e10f,1,false);
			AppendIn(propGrid,propGridSegmentSize);
		}

		propRenderHelpers = new BoolRefProperty("Helpers","Helpers are non-scene elements rendered with scene, usually for diagnostic purposes.",svs.renderHelpers);
		AppendIn(propRenderExtras,propRenderHelpers);

		SetPropertyBackgroundColour(propRenderExtras,headerColor,false);
	}

	// GI quality
	{
		wxPGProperty* propGI = new wxStringProperty(wxT("GI quality"), wxPG_LABEL);
		Append(propGI);
		SetPropertyReadOnly(propGI,true,wxPG_DONT_RECURSE);

		propGIFireballQuality = new FloatProperty("Fireball quality","More = longer precalculation, higher quality realtime GI. Rebuild Fireball for this change to take effect.",svs.fireballQuality,0,0,1000000,100,false);
		AppendIn(propGI,propGIFireballQuality);

		// cubes
		{
			propGIRaytracedCubes = new BoolRefProperty("Realtime raytraced reflections","Increases realism by realtime raytracing diffuse and specular reflection cubemaps.",svs.raytracedCubesEnabled);
			AppendIn(propGI,propGIRaytracedCubes);
		
			propGIRaytracedCubesDiffuseRes = new FloatProperty("Diffuse resolution","Resolution of diffuse reflection cube maps (total size is x*x*6 pixels). Applied only to dynamic objects. More = higher quality, slower. Default=4.",svs.raytracedCubesDiffuseRes,0,1,16,1,false);
			AppendIn(propGIRaytracedCubes,propGIRaytracedCubesDiffuseRes);

			propGIRaytracedCubesSpecularRes = new FloatProperty("Specular resolution","Resolution of specular reflection cube maps (total size is x*x*6 pixels). More = higher quality, slower. Default=16.",svs.raytracedCubesSpecularRes,0,1,64,1,false);
			AppendIn(propGIRaytracedCubes,propGIRaytracedCubesSpecularRes);

			propGIRaytracedCubesMaxObjects = new FloatProperty("Max objects","How many objects in scene before raytracing turns off automatically. Raytracing usually becomes bottleneck when there are more than 1000 objects.",svs.raytracedCubesMaxObjects,0,0,1000000,10,false);
			AppendIn(propGIRaytracedCubes,propGIRaytracedCubesMaxObjects);
		}

		propGIEmisMultiplier = new FloatProperty("Emissive multiplier","Multiplies effect of emissive materials on scene, without affecting emissive materials. Default=1.",svs.emissiveMultiplier,svs.precision,0,1e10f,1,false);
		AppendIn(propGI,propGIEmisMultiplier);

		propGIEmisVideoAffectsGI = new BoolRefProperty("Emissive video realtime GI","Makes video in emissive material slot affect GI in realtime, light emitted from video is recalculated in every frame.",svs.videoEmittanceAffectsGI);
		AppendIn(propGI,propGIEmisVideoAffectsGI);

		propGITranspVideoAffectsGI = new BoolRefProperty("Transparency video realtime GI","Makes video in transparency material slot affect GI in realtime, light going through transparent regions is recalculated in every frame.",svs.videoTransmittanceAffectsGI);
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

	propEnvMap->Hide(svs.envSimulateSky,false);
	propEnvLocation->Hide(!svs.envSimulateSky&&!svs.envSimulateSun);
	propEnvDate->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);
	propEnvTime->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);

	propToneMappingAutomatic->Hide(!svs.renderTonemapping,false);
	propToneMappingAutomaticTarget->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingAutomaticSpeed->Hide(!svs.renderTonemapping || !svs.tonemappingAutomatic,false);
	propToneMappingBrightness->Hide(!svs.renderTonemapping,false);
	propToneMappingContrast->Hide(!svs.renderTonemapping,false);

	propWaterColor->Hide(!svs.renderWater,false);
	propWaterLevel->Hide(!svs.renderWater,false);

	propLensFlareSize->Hide(!svs.renderLensFlare,false);
	propLensFlareId->Hide(!svs.renderLensFlare,false);

	propGridNumSegments->Hide(!svs.renderGrid,false);
	propGridSegmentSize->Hide(!svs.renderGrid,false);

	propGIRaytracedCubesDiffuseRes->Hide(!svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesSpecularRes->Hide(!svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesMaxObjects->Hide(!svs.raytracedCubesEnabled,false);
}

void SVSceneProperties::updateProperties()
{
	// This function can update any property that has suddenly changed in svs, outside property grid.
	// We depend on it when new svs is loaded from disk.
	// Other approach would be to create all properties again, however,
	// this function would still have to support at least properties that user can change by hotkeys or mouse navigation.
	unsigned numChangesRelevantForHiding =
		+ updateBoolRef(propCameraOrtho)
		//+ updateBoolRef(propEnvSimulateSky)
		+ updateBoolRef(propEnvSimulateSun)
		+ updateBoolRef(propToneMapping)
		+ updateBool(propToneMappingAutomatic,svs.tonemappingAutomatic)
		+ updateBoolRef(propWater)
		+ updateBoolRef(propLensFlare)
		+ updateBoolRef(propGrid)
		;
	unsigned numChangesOther =
		+ updateFloat(propCameraSpeed,svs.cameraMetersPerSecond)
		+ updateProperty(propCameraPosition,svs.eye.pos)
		+ updateProperty(propCameraAngles,RR_RAD2DEG(RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle)))
		+ updateInt(propCameraView,view2ME_VIEW(svs.eye.getView()))
		+ updateFloat(propCameraOrthoSize,svs.eye.orthoSize)
		+ updateFloat(propCameraFov,svs.eye.getFieldOfViewVerticalDeg())
		+ updateFloat(propCameraNear,svs.eye.getNear())
		+ updateFloat(propCameraFar,svs.eye.getFar())
		+ updateProperty(propCameraCenter,svs.eye.screenCenter)
		//+ updateBoolRef(propEnvSimulateSky)
		+ updateBoolRef(propEnvSimulateSun)
		+ updateString(propEnvMap,(svframe->m_canvas&&svframe->m_canvas->solver&&svframe->m_canvas->solver->getEnvironment())?svframe->m_canvas->solver->getEnvironment()->filename.c_str():"(no texture)")
		+ updateProperty(propEnvLocation,rr::RRVec2(svs.envLatitudeDeg,svs.envLongitudeDeg))
		+ updateDate(propEnvDate,wxDateTime(svs.envDateTime))
		+ updateFloat(propEnvTime,svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.f)
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
		+ updateBoolRef(propRenderBloom)
		+ updateFloat(propLensFlareSize,svs.lensFlareSize)
		+ updateFloat(propLensFlareId,svs.lensFlareId)
		+ updateBoolRef(propRenderVignette)
		+ updateProperty(propWaterColor,svs.waterColor)
		+ updateFloat(propWaterLevel,svs.waterLevel)
		+ updateInt(propGridNumSegments,svs.gridNumSegments)
		+ updateFloat(propGridSegmentSize,svs.gridSegmentSize)
		+ updateInt(propGIFireballQuality,svs.fireballQuality)
		+ updateBoolRef(propGIRaytracedCubes)
		+ updateInt(propGIRaytracedCubesDiffuseRes,svs.raytracedCubesDiffuseRes)
		+ updateInt(propGIRaytracedCubesSpecularRes,svs.raytracedCubesSpecularRes)
		+ updateInt(propGIRaytracedCubesMaxObjects,svs.raytracedCubesMaxObjects)
		+ updateFloat(propGIEmisMultiplier,svs.emissiveMultiplier)
		+ updateBoolRef(propGIEmisVideoAffectsGI)
		+ updateBoolRef(propGITranspVideoAffectsGI)
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
		svframe->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,menuCode));
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
		svs.skyboxFilename = propEnvMap->GetValue().GetString().c_str();
		svframe->OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_ENV_RELOAD));
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
		svframe->simulateSun();
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
	else
	if (property==propGIFireballQuality)
	{
		svs.fireballQuality = property->GetValue().GetInteger();
	}
	else
	if (property==propGIRaytracedCubes)
	{
		updateHide();
	}
	else
	if (property==propGIRaytracedCubesDiffuseRes || property==propGIRaytracedCubesSpecularRes)
	{
		svs.raytracedCubesDiffuseRes = propGIRaytracedCubesDiffuseRes->GetValue().GetInteger();
		svs.raytracedCubesSpecularRes = propGIRaytracedCubesSpecularRes->GetValue().GetInteger();
		svframe->m_canvas->reallocateBuffersForRealtimeGI(false);
	}
	else
	if (property==propGIRaytracedCubesMaxObjects)
	{
		svs.raytracedCubesMaxObjects = property->GetValue().GetInteger();
	}
	else
	if (property==propGIEmisMultiplier)
	{
		svs.emissiveMultiplier = property->GetValue().GetDouble();
		if (svframe->m_canvas->solver)
			svframe->m_canvas->solver->setEmittance(svs.emissiveMultiplier,16,true);
	}
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
