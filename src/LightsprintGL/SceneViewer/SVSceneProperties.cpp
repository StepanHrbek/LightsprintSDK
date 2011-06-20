// --------------------------------------------------------------------------
// Scene viewer - scene properties window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVSceneProperties.h"
#include "SVFrame.h" // solver access
#include "SVCanvas.h" // solver access
#include "SVSceneTree.h" // action buttons
#include "../Workaround.h"
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
	// camera
	{
		propCamera = new wxStringProperty(_("Camera"), wxPG_LABEL);
		Append(propCamera);
		SetPropertyReadOnly(propCamera,true,wxPG_DONT_RECURSE);


		propCameraSpeed = new FloatProperty(_("Speed")+" (m/s)",_("Controls how quickly camera moves when controlled by arrows/wsad."),svs.cameraMetersPerSecond,svs.precision,0,1e10f,1,false);
		AppendIn(propCamera,propCameraSpeed);

		const wxChar* viewStrings[] = {_("Custom"),_("Top"),_("Bottom"),_("Front"),_("Back"),_("Left"),_("Right"),NULL};
		const long viewValues[] = {SVFrame::ME_VIEW_RANDOM,SVFrame::ME_VIEW_TOP,SVFrame::ME_VIEW_BOTTOM,SVFrame::ME_VIEW_FRONT,SVFrame::ME_VIEW_BACK,SVFrame::ME_VIEW_LEFT,SVFrame::ME_VIEW_RIGHT};
		propCameraView = new wxEnumProperty(_("View"), wxPG_LABEL, viewStrings, viewValues);
		AppendIn(propCamera,propCameraView);

		propCameraPosition = new RRVec3Property(_("Position")+" (m)",_("Camera position in world space"),svs.precision,svs.eye.pos,1);
		AppendIn(propCamera,propCameraPosition);

		propCameraDirection = new RRVec3Property(_("Direction"),_("Camera direction in world space, normalized"),svs.precision,svs.eye.dir,0.2f);
		AppendIn(propCamera,propCameraDirection);
		EnableProperty(propCameraDirection,false);

		propCameraAngles = new RRVec3Property(_("Angles")+L" (\u00b0)",_("Camera direction in angles: azimuth, elevation, leaning"),svs.precision,RR_RAD2DEG(RRVec3(svs.eye.angle,svs.eye.angleX,svs.eye.leanAngle)),10);
		AppendIn(propCamera,propCameraAngles);

		propCameraOrtho = new BoolRefProperty(_("Orthogonal"),_("Switches between orthogonal and perspective camera."),svs.eye.orthogonal);
		AppendIn(propCamera,propCameraOrtho);

		propCameraOrthoSize = new FloatProperty(_("Size")+" (m)",_("World space distance between top and bottom of viewport."),svs.eye.orthoSize,svs.precision,0,1000000,10,false);
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

		propCameraCenter = new RRVec2Property(_("Center of screen"),_("Shifts look up/down/left/right without distorting image. E.g. in architecture, 0,0.3 moves horizon down without skewing vertical lines."),svs.precision,svs.eye.screenCenter,1);
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

		//propEnvSimulateSky = new BoolRefProperty(_("Simulate sky"),_("Work in progress, has no effect."),svs.envSimulateSky);
		//AppendIn(propEnv,propEnvSimulateSky);

		propEnvSimulateSun = new BoolRefProperty(_("Simulate Sun"),_("Calculates Sun position from date, time and location. Affects first directional light only, insert one if none exists. World directions: north=Z+, east=X+, up=Y+."),svs.envSimulateSun);
		AppendIn(propEnv,propEnvSimulateSun);

		propEnvLocation = new LocationProperty(_("Location"),_("Geolocation used for Sun and sky simulation."),svs.precision,rr::RRVec2(svs.envLatitudeDeg,svs.envLongitudeDeg));
		AppendIn(propEnv,propEnvLocation);

		propEnvDate = new wxDateProperty(_("Date"),wxPG_LABEL,wxDateTime(svs.envDateTime));
		propEnvDate->SetHelpString(_("Date used for Sun and sky simulation."));
		AppendIn(propEnv,propEnvDate);

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

		propRenderMaterialTextures = new BoolRefProperty(_("Textures"),_("Toggles between material textures and flat colors. Disabling textures could make rendering faster.")+" (ctrl-t)",svs.renderMaterialTextures);
		AppendIn(propRenderMaterials,propRenderMaterialTextures);

		SetPropertyBackgroundColour(propRenderMaterials,importantPropertyBackgroundColor,false);
	}

	// render extras
	{
		propRenderExtras = new wxStringProperty(_("Render extras"), wxPG_LABEL);
		Append(propRenderExtras);
		SetPropertyReadOnly(propRenderExtras,true,wxPG_DONT_RECURSE);

		// water
		{
			propWater = new BoolRefProperty(_("Water"),_("Enables rendering of water layer, with reflection and waves."),svs.renderWater);
			AppendIn(propRenderExtras,propWater);

			propWaterColor = new HDRColorProperty(_("Color"),_("Color of scattered light coming out of water."),svs.precision,svs.waterColor);
			AppendIn(propWater,propWaterColor);

			propWaterLevel = new FloatProperty(_("Level"),_("Altitude of water surface, Y coordinate in world space."),svs.waterLevel,svs.precision,-1e10f,1e10f,1,false);
			AppendIn(propWater,propWaterLevel);
		}

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

	// global illumination
	{
		wxPGProperty* propGI = new wxStringProperty(_("Global illumination"), wxPG_LABEL);
		Append(propGI);
		SetPropertyReadOnly(propGI,true,wxPG_DONT_RECURSE);

		// direct
		{
			const wxChar* strings[] = {_("realtime"),_("static lightmap"),_("none"),NULL};
			const long values[] = {LD_REALTIME,LD_STATIC_LIGHTMAPS,LD_NONE};
			propGIDirect = new wxEnumProperty(_("Direct illumination"),wxPG_LABEL,strings,values);
			propGIDirect->SetHelpString(_("What direct illumination technique to use."));
			AppendIn(propGI,propGIDirect);

			{
				const wxChar* strings[] = {_("0-bit (opaque shadows)"),_("1-bit (alpha keyed shadows)"),_("24-bit (rgb shadows)"),NULL};
				const long values[] = {RealtimeLight::FULLY_OPAQUE_SHADOWS,RealtimeLight::ALPHA_KEYED_SHADOWS,RealtimeLight::RGB_SHADOWS};
				propGIShadowTransparency = new wxEnumProperty(_("Shadow transparency"),wxPG_LABEL,strings,values);
				propGIShadowTransparency->SetHelpString(_("Changes how realistically semi-transparent shadows are rendered."));
				AppendIn(propGIDirect,propGIShadowTransparency);
			}

			propGISRGBCorrect = new BoolRefProperty(_("sRGB correctness"),_("Increases realism by correctly adding realtime lights. Works only if OpenGL 3.0+ or necessary extensions are found."),svs.srgbCorrect);
			AppendIn(propGIDirect,propGISRGBCorrect);
		}

		// indirect
		{
			const wxChar* strings[] = {_("realtime Fireball (fast)"),_("realtime Architect (no precalc)"),_("static lightmap"),_("constant ambient"),_("none"),NULL};
			const long values[] = {LI_REALTIME_FIREBALL,LI_REALTIME_ARCHITECT,LI_STATIC_LIGHTMAPS,LI_CONSTANT,LI_NONE};
			propGIIndirect = new wxEnumProperty(_("Indirect illumination"),wxPG_LABEL,strings,values);
			propGIIndirect->SetHelpString(_("What indirect illumination technique to use."));
			AppendIn(propGI,propGIIndirect);

			propGILDM = new BoolRefProperty(_("LDM"),_("Light detail maps improve quality of constant and realtime indirect illumination."),svs.renderLDM);
			AppendIn(propGIIndirect,propGILDM);

			// fireball
			{
				wxPGProperty* propGIFireball = new wxStringProperty(_("Fireball"), wxPG_LABEL);
				AppendIn(propGIIndirect,propGIFireball);
				SetPropertyReadOnly(propGIFireball,true,wxPG_DONT_RECURSE);

				propGIFireballQuality = new FloatProperty(_("Quality"),_("More = longer precalculation, higher quality realtime GI. Rebuild Fireball for this change to take effect."),svs.fireballQuality,0,0,1000000,100,false);
				AppendIn(propGIFireball,propGIFireballQuality);

				propGIFireballBuild = new ButtonProperty(_("Build"),_("Builds or rebuilds Fireball."),svframe,SVFrame::ME_REALTIME_FIREBALL_BUILD);
				AppendIn(propGIFireball,propGIFireballBuild);
				propGIFireballBuild->updateImage();
			}

			// cubes
			{
				propGIRaytracedCubes = new BoolRefProperty(_("Realtime raytraced reflections"),_("Increases realism by realtime raytracing diffuse and specular reflection cubemaps."),svs.raytracedCubesEnabled);
				AppendIn(propGIIndirect,propGIRaytracedCubes);
		
				propGIRaytracedCubesDiffuseRes = new FloatProperty(_("Diffuse resolution"),_("Resolution of diffuse reflection cube maps (total size is x*x*6 pixels). Applied only to dynamic objects. More = higher quality, slower. Default=4."),svs.raytracedCubesDiffuseRes,0,1,16,1,false);
				AppendIn(propGIRaytracedCubes,propGIRaytracedCubesDiffuseRes);

				propGIRaytracedCubesSpecularRes = new FloatProperty(_("Specular resolution"),_("Resolution of specular reflection cube maps (total size is x*x*6 pixels). More = higher quality, slower. Default=16."),svs.raytracedCubesSpecularRes,0,1,64,1,false);
				AppendIn(propGIRaytracedCubes,propGIRaytracedCubesSpecularRes);

				propGIRaytracedCubesMaxObjects = new FloatProperty(_("Max objects"),_("How many objects in scene before raytracing turns off automatically. Raytracing usually becomes bottleneck when there are more than 1000 objects."),svs.raytracedCubesMaxObjects,0,0,1000000,10,false);
				AppendIn(propGIRaytracedCubes,propGIRaytracedCubesMaxObjects);

				propGIRaytracedCubesSpecularThreshold = new FloatProperty(_("Specular threshold"),_("Only objects with specular color above threshold apply for specular cube reflection, 0=all objects apply, 1=only objects with spec color 1 apply."),svs.raytracedCubesSpecularThreshold,svs.precision,0,10,0.1f,false);
				AppendIn(propGIRaytracedCubesSpecularRes,propGIRaytracedCubesSpecularThreshold);

				propGIRaytracedCubesDepthThreshold = new FloatProperty(_("Depth threshold"),_("Only objects with depth above threshold apply for specular cube reflection, 0=all objects apply, 0.1=all but near planar objects apply, 1=none apply."),svs.raytracedCubesDepthThreshold,svs.precision,0,1,0.1f,false);
				AppendIn(propGIRaytracedCubesSpecularRes,propGIRaytracedCubesDepthThreshold);
			}

			propGIEmisMultiplier = new FloatProperty(_("Emissive multiplier"),_("Multiplies effect of emissive materials on scene, without affecting emissive materials. Default=1."),svs.emissiveMultiplier,svs.precision,0,1e10f,1,false);
			AppendIn(propGIIndirect,propGIEmisMultiplier);

			// video
			{
				propGIVideo = new wxStringProperty(_("Video realtime GI"), wxPG_LABEL);
				AppendIn(propGIIndirect,propGIVideo);
				SetPropertyReadOnly(propGIVideo,true,wxPG_DONT_RECURSE);

				// emissive video
				{
					propGIEmisVideoAffectsGI = new BoolRefProperty(_("Emissive"),_("Makes video in emissive material slot affect GI in realtime, light emitted from video is recalculated in every frame."),svs.videoEmittanceAffectsGI);
					AppendIn(propGIVideo,propGIEmisVideoAffectsGI);

					propGIEmisVideoGIQuality = new FloatProperty(_("Quality"),_("Number of samples taken from each triangle."),svs.videoEmittanceGIQuality,0,0,1000,10,false);
					AppendIn(propGIEmisVideoAffectsGI,propGIEmisVideoGIQuality);
				}

				// transmittance video
				{
					propGITranspVideoAffectsGI = new BoolRefProperty(_("Transparency"),_("Makes video in transparency material slot affect GI in realtime, light going through transparent regions is recalculated in every frame."),svs.videoTransmittanceAffectsGI);
					AppendIn(propGIVideo,propGITranspVideoAffectsGI);

					propGITranspVideoAffectsGIFull = new BoolRefProperty(_("Full GI"),_("Full GI is updated rather than just shadows."),svs.videoTransmittanceAffectsGIFull);
					AppendIn(propGITranspVideoAffectsGI,propGITranspVideoAffectsGIFull);
				}

				// environment video
				{
					propGIEnvVideoAffectsGI = new BoolRefProperty(_("Environment"),_("Makes video in environment affect GI in realtime, light emitted from video is recalculated in every frame."),svs.videoEnvironmentAffectsGI);
					AppendIn(propGIVideo,propGIEnvVideoAffectsGI);

					propGIEnvVideoGIQuality = new FloatProperty(_("Quality"),_("Number of samples taken from environment."),svs.videoEnvironmentGIQuality,0,1,100000,200,false);
					AppendIn(propGIEnvVideoAffectsGI,propGIEnvVideoGIQuality);
				}

				Collapse(propGIVideo);
			}
		}

		// lightmap
		{
			wxPGProperty* propGILightmap = new wxStringProperty(_("Lightmap/LDM baking"), wxPG_LABEL);
			AppendIn(propGI,propGILightmap);
			SetPropertyReadOnly(propGILightmap,true,wxPG_DONT_RECURSE);
			
			propGILightmapAOIntensity = new FloatProperty(_("AO intensity"),_("Higher value makes indirect illumination in corners darker, 0=disabled/lighter, 1=normal, 2=darker."),svs.lightmapDirectParameters.aoIntensity,svs.precision,0,10,1,false);
			AppendIn(propGILightmap,propGILightmapAOIntensity);

			propGILightmapAOSize = new FloatProperty(_("AO size")+" (m)",_("Indirect illumination gets darker in this distance from corners, 0=disabled. If set too high, indirect illumination becomes completely black."),svs.lightmapDirectParameters.aoSize,svs.precision,0,1000,1,false);
			AppendIn(propGILightmap,propGILightmapAOSize);

			propGILightmapSmoothingAmount = new FloatProperty(_("Smoothing amount"),_("Amount of smoothing applied when baking lightmaps. Makes edges smoother, reduces noise, but washes out tiny details. Reasonable values are around 1. 0=off."),svs.lightmapFilteringParameters.smoothingAmount,svs.precision,0,10,1,false);
			AppendIn(propGILightmap,propGILightmapSmoothingAmount);

			propGILightmapWrapping = new BoolRefProperty(_("Wrapping"),_("Checked = smoothing works across lightmap boundaries."),svs.lightmapFilteringParameters.wrap);
			AppendIn(propGILightmap,propGILightmapWrapping);

			propGIBuildLmaps = new ButtonProperty(_("Build lightmaps"),_("Builds or rebuilds lightmaps for all static objects."),svframe,CM_STATIC_OBJECTS_BUILD_LMAPS);
			AppendIn(propGILightmap,propGIBuildLmaps);
			propGIBuildLmaps->updateImage();

			propGIBuildLDMs = new ButtonProperty(_("Build LDMs"),_("Builds or rebuilds LDMs for all static objects."),svframe,CM_STATIC_OBJECTS_BUILD_LDMS);
			AppendIn(propGILightmap,propGIBuildLDMs);
			propGIBuildLDMs->updateImage();
			propGIBilinear = new BoolRefProperty(_("Bilinear"),_("Bilinear interpolation of lightmaps and LDMs, keep always on, unless you analyze pixels."),svs.renderLightmapsBilinear);
			AppendIn(propGILightmap,propGIBilinear);
		}

		SetPropertyBackgroundColour(propGI,importantPropertyBackgroundColor,false);
	}

	updateHide();
}

//! Copy svs -> hide/show property.
//! Must not be called in every frame, float property that is unhid in every frame loses focus immediately after click, can't be edited.
void SVSceneProperties::updateHide()
{
	propCameraFov->Hide(svs.eye.orthogonal,false);
	propCameraOrthoSize->Hide(!svs.eye.orthogonal,false);
	EnableProperty(propCameraNear,!svs.cameraDynamicNear);
	EnableProperty(propCameraFar,!svs.cameraDynamicNear);

	propEnvMap->Hide(svs.envSimulateSky,false);
	propEnvLocation->Hide(!svs.envSimulateSky&&!svs.envSimulateSun);
	propEnvDate->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);
	propEnvTime->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);
	propEnvSpeed->Hide(!svs.envSimulateSky&&!svs.envSimulateSun,false);

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

	bool realtimeGI = svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT;
	propGILDM->Hide(svs.renderLightIndirect==LI_NONE || svs.renderLightIndirect==LI_STATIC_LIGHTMAPS,false);
	propGISRGBCorrect->Hide(svs.renderLightDirect!=LD_REALTIME,false);
	propGIShadowTransparency->Hide(svs.renderLightDirect!=LD_REALTIME,false);

	propGIRaytracedCubes->Hide(!realtimeGI,false);
	propGIRaytracedCubesDiffuseRes->Hide(!realtimeGI || !svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesSpecularRes->Hide(!realtimeGI || !svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesMaxObjects->Hide(!realtimeGI || !svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesSpecularThreshold->Hide(!realtimeGI || !svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesDepthThreshold->Hide(!realtimeGI || !svs.raytracedCubesEnabled,false);
	
	propGIEmisMultiplier->Hide(!realtimeGI,false);
	propGIVideo->Hide(!realtimeGI,false);
	propGIEmisVideoAffectsGI->Hide(!realtimeGI,false);
	propGIEmisVideoGIQuality->Hide(!realtimeGI || !svs.videoEmittanceAffectsGI,false);
	propGITranspVideoAffectsGI->Hide(svs.renderLightDirect!=LD_REALTIME,false);
	propGITranspVideoAffectsGIFull->Hide(!realtimeGI || !svs.videoTransmittanceAffectsGI,false);
	propGIEnvVideoAffectsGI->Hide(svs.renderLightIndirect!=LI_REALTIME_FIREBALL,false);
	propGIEnvVideoGIQuality->Hide(svs.renderLightIndirect!=LI_REALTIME_FIREBALL || !svs.videoEnvironmentAffectsGI,false);

}

void SVSceneProperties::updateAfterGLInit()
{
	propGISRGBCorrect->Hide(!Workaround::supportsSRGB(),false);
}

void SVSceneProperties::updateProperties()
{
	// This function can update any property that has suddenly changed in svs, outside property grid.
	// We depend on it when new svs is loaded from disk.
	// Other approach would be to create all properties again, however,
	// this function would still have to support at least properties that user can change by hotkeys or mouse navigation.
	unsigned numChangesRelevantForHiding =
		+ updateBoolRef(propCameraOrtho)
		+ updateBoolRef(propCameraRangeAutomatic)
		//+ updateBoolRef(propEnvSimulateSky)
		+ updateBoolRef(propEnvSimulateSun)
		+ updateBoolRef(propToneMapping)
		+ updateBool(propToneMappingAutomatic,svs.tonemappingAutomatic)
		+ updateBoolRef(propWater)
		+ updateBoolRef(propLogo)
		+ updateBoolRef(propLensFlare)
		+ updateBoolRef(propVignette)
		+ updateBoolRef(propGrid)
		+ updateInt(propGIDirect,svs.renderLightDirect)
		+ updateInt(propGIIndirect,svs.renderLightIndirect)
		+ updateBoolRef(propGILDM)
		+ updateBoolRef(propGIEmisVideoAffectsGI)
		+ updateBoolRef(propGITranspVideoAffectsGI)
		+ updateBoolRef(propGIEnvVideoAffectsGI)
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
		+ updateString(propEnvMap,(svframe->m_canvas&&svframe->m_canvas->solver&&svframe->m_canvas->solver->getEnvironment())?RR_RR2WX(svframe->m_canvas->solver->getEnvironment()->filename):L"(no texture)")
		+ updateProperty(propEnvLocation,rr::RRVec2(svs.envLatitudeDeg,svs.envLongitudeDeg))
		+ updateDate(propEnvDate,wxDateTime(svs.envDateTime))
		+ updateFloat(propEnvTime,svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.f)
		+ updateFloat(propEnvSpeed,svs.envSpeed)
		+ updateFloat(propToneMappingAutomaticTarget,svs.tonemappingAutomaticTarget)
		+ updateFloat(propToneMappingAutomaticSpeed,svs.tonemappingAutomaticSpeed)
		+ updateFloat(propToneMappingBrightness,svs.tonemappingBrightness[0])
		+ updateFloat(propToneMappingContrast,svs.tonemappingGamma)
		+ updateBoolRef(propRenderMaterialDiffuse)
		+ updateBoolRef(propRenderMaterialSpecular)
		+ updateBoolRef(propRenderMaterialEmittance)
		+ updateInt(propRenderMaterialTransparency,svs.renderMaterialTransparency)
		+ updateBoolRef(propRenderMaterialTextures)
		+ updateBoolRef(propRenderWireframe)
		+ updateBoolRef(propRenderHelpers)
		+ updateBoolRef(propRenderFPS)
		+ updateBoolRef(propRenderBloom)
		+ updateFloat(propLensFlareSize,svs.lensFlareSize)
		+ updateFloat(propLensFlareId,svs.lensFlareId)
		+ updateProperty(propWaterColor,svs.waterColor)
		+ updateFloat(propWaterLevel,svs.waterLevel)
		+ updateInt(propGridNumSegments,svs.gridNumSegments)
		+ updateFloat(propGridSegmentSize,svs.gridSegmentSize)
		+ updateBoolRef(propGISRGBCorrect)
		+ updateInt(propGIShadowTransparency,svs.shadowTransparency)
		+ updateInt(propGIFireballQuality,svs.fireballQuality)
		+ updateBoolRef(propGIRaytracedCubes)
		+ updateInt(propGIRaytracedCubesDiffuseRes,svs.raytracedCubesDiffuseRes)
		+ updateInt(propGIRaytracedCubesSpecularRes,svs.raytracedCubesSpecularRes)
		+ updateInt(propGIRaytracedCubesMaxObjects,svs.raytracedCubesMaxObjects)
		+ updateFloat(propGIRaytracedCubesSpecularThreshold,svs.raytracedCubesSpecularThreshold)
		+ updateFloat(propGIRaytracedCubesDepthThreshold,svs.raytracedCubesDepthThreshold)
		+ updateFloat(propGIEmisMultiplier,svs.emissiveMultiplier)
		+ updateInt(propGIEmisVideoGIQuality,svs.videoEmittanceGIQuality)
		+ updateBoolRef(propGITranspVideoAffectsGIFull)
		+ updateInt(propGIEnvVideoGIQuality,svs.videoEnvironmentGIQuality)
		+ updateFloat(propGILightmapAOIntensity,svs.lightmapDirectParameters.aoIntensity)
		+ updateFloat(propGILightmapAOSize,svs.lightmapDirectParameters.aoSize)
		+ updateFloat(propGILightmapSmoothingAmount,svs.lightmapFilteringParameters.smoothingAmount)
		+ updateBoolRef(propGILightmapWrapping)
		+ updateBoolRef(propGIBilinear)
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
		defocusButtonEditor();
	}
}

void SVSceneProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

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
	if (property==propCameraRangeAutomatic)
	{
		updateHide();
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
		svs.skyboxFilename = RR_WX2RR(propEnvMap->GetValue().GetString());
		svframe->OnMenuEventCore(SVFrame::ME_ENV_RELOAD);
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
	if (property==propGIDirect)
	{
		svs.renderLightDirect = (LightingDirect)property->GetValue().GetInteger();
		if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS)
			svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
		if (svs.renderLightDirect!=LD_STATIC_LIGHTMAPS && svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)
			svs.renderLightIndirect = LI_CONSTANT;
		updateHide();
	}
	else
	if (property==propGIIndirect)
	{
		svs.renderLightIndirect = (LightingIndirect)property->GetValue().GetInteger();
		if (svs.renderLightIndirect==LI_STATIC_LIGHTMAPS)
			svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
		if (svs.renderLightIndirect!=LI_STATIC_LIGHTMAPS && svs.renderLightDirect==LD_STATIC_LIGHTMAPS)
			svs.renderLightDirect = LD_REALTIME;
		if (svs.renderLightIndirect==LI_REALTIME_FIREBALL)
			svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_FIREBALL);
		if (svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
			svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_ARCHITECT);
		updateHide();
	}
	else
	if (property==propGILDM)
	{
		updateHide();
	}
	else
	if (property==propGIShadowTransparency)
	{
		svs.shadowTransparency = (RealtimeLight::ShadowTransparency)property->GetValue().GetInteger();
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
		svframe->m_canvas->reallocateBuffersForRealtimeGI(false);
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
	if (property==propGIRaytracedCubesSpecularThreshold)
	{
		svs.raytracedCubesSpecularThreshold = property->GetValue().GetDouble();
		svframe->m_canvas->reallocateBuffersForRealtimeGI(false);
	}
	else
	if (property==propGIRaytracedCubesDepthThreshold)
	{
		svs.raytracedCubesDepthThreshold = property->GetValue().GetDouble();
		svframe->m_canvas->reallocateBuffersForRealtimeGI(false);
	}
	else
	if (property==propGIEmisMultiplier)
	{
		svs.emissiveMultiplier = property->GetValue().GetDouble();
	}
	else
	if (property==propGIEmisVideoAffectsGI || property==propGITranspVideoAffectsGI || property==propGIEnvVideoAffectsGI)
	{
		updateHide();
	}
	else
	if (property==propGIEmisVideoGIQuality)
	{
		svs.videoEmittanceGIQuality = property->GetValue().GetInteger();
	}
	else
	if (property==propGIEnvVideoGIQuality)
	{
		svs.videoEnvironmentGIQuality = property->GetValue().GetInteger();
	}
	else
	if (property==propGILightmapAOIntensity)
	{
		svs.lightmapDirectParameters.aoIntensity = property->GetValue().GetDouble();
	}
	else
	if (property==propGILightmapAOSize)
	{
		svs.lightmapDirectParameters.aoSize = property->GetValue().GetDouble();
	}
	else
	if (property==propGILightmapSmoothingAmount)
	{
		svs.lightmapFilteringParameters.smoothingAmount = property->GetValue().GetDouble();
	}
	else
	if (property==propGIBilinear)
	{
		for (unsigned i=0;i<svframe->m_canvas->solver->getStaticObjects().size();i++)
		{	
			for (unsigned j=0;j<2;j++)
			{
				rr::RRBuffer* buf = svframe->m_canvas->solver->getStaticObjects()[i]->illumination.getLayer(j?svs.staticLayerNumber:svs.ldmLayerNumber);
				if (buf && buf->getType()==rr::BT_2D_TEXTURE)
				{
					getTexture(buf)->bindTexture();
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, svs.renderLightmapsBilinear?GL_LINEAR:GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, svs.renderLightmapsBilinear?GL_LINEAR:GL_NEAREST);
				}
			}
		}
	}
}

BEGIN_EVENT_TABLE(SVSceneProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVSceneProperties::OnPropertyChange)
	EVT_IDLE(SVSceneProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
