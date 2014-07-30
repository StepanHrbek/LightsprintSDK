// --------------------------------------------------------------------------
// Scene viewer - GI properties window.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVGIProperties.h"
#include "SVSceneTree.h" // action buttons

namespace rr_ed
{

extern bool supportsSRGB();

SVGIProperties::SVGIProperties(SVFrame* _svframe)
	: SVProperties(_svframe)
{
	// direct
	{
		propGIDirect = new BoolRefProperty(_("Direct illumination"),_("Uncheck to disable direct illumination, e.g. to see indirect illumination more clearly."),svs.renderLightDirect);
		Append(propGIDirect);

		{
			const wxChar* strings[] = {_("0-bit (opaque shadows)"),_("1-bit (alpha keyed shadows)"),_("24-bit (rgb shadows)"),_("24-bit (fresnel shadows)"),NULL};
			const long values[] = {rr_gl::RealtimeLight::FULLY_OPAQUE_SHADOWS,rr_gl::RealtimeLight::ALPHA_KEYED_SHADOWS,rr_gl::RealtimeLight::RGB_SHADOWS,rr_gl::RealtimeLight::FRESNEL_SHADOWS};
			propGIShadowTransparency = new wxEnumProperty(_("Shadow transparency"),wxPG_LABEL,strings,values);
			propGIShadowTransparency->SetHelpString(_("Changes how realistically semi-transparent shadows are rendered."));
			AppendIn(propGIDirect,propGIShadowTransparency);
		}

		propGISRGBCorrect = new BoolRefProperty(_("sRGB correctness"),_("Increases realism by correctly adding realtime lights (however, transparency in sRGB mode looks different). Works only if OpenGL 3.0+ or necessary extensions are found."),svs.srgbCorrect);
		AppendIn(propGIDirect,propGISRGBCorrect);

		SetPropertyBackgroundColour(propGIDirect,importantPropertyBackgroundColor,false);
	}

	// indirect
	{
		const wxChar* strings[] = {_("pathtraced"),_("realtime Fireball (fast)"),_("realtime Architect (no precalc)"),_("lightmaps"),_("ambient maps"),_("constant ambient"),_("none"),NULL};
		const long values[] = {LI_PATHTRACED,LI_REALTIME_FIREBALL,LI_REALTIME_ARCHITECT,LI_LIGHTMAPS,LI_AMBIENTMAPS,LI_CONSTANT,LI_NONE};
		propGIIndirect = new wxEnumProperty(_("Indirect illumination"),wxPG_LABEL,strings,values);
		propGIIndirect->SetHelpString(_("What nondirectional indirect illumination technique to use. Note that directional techniques (Raytraced cubemaps, Mirror reflections) are enabled separately, so even if you set 'constant ambient' or 'none' here, you might still see indirect light from cubemaps or mirrors."));
		Append(propGIIndirect);

		propGIIndirectMultiplier = new FloatProperty(_("Light multiplier"),_("Multiplies indirect illumination from lights, without affecting lights. 1=realistic. In baked modes, it is applied when baking, not when rendering. Not applied in constant mode."),svs.renderLightIndirectMultiplier,svs.precision,0,10000,1,false);
		AppendIn(propGIIndirect,propGIIndirectMultiplier);

		propGIEmisMultiplier = new FloatProperty(_("Emissive multiplier"),_("Multiplies effect of emissive materials on scene, without affecting materials. 1=realistic. In baked modes, it is applied when baking, not when rendering."),svs.emissiveMultiplier,svs.precision,0,1e10f,1,false);
		AppendIn(propGIIndirect,propGIEmisMultiplier);

		// SSGI
		{
			propGISSGI = new BoolRefProperty(_("SSGI"),_("Screen space global illumination improves quality of constant and realtime indirect illumination. SSGI works without any precalculations, however, it is slower and looks worse than LDM."),svs.ssgiEnabled);
			AppendIn(propGIIndirect,propGISSGI);

			propGISSGIIntensity = new FloatProperty(_("Intensity"),_("Multiplies effect of SSGI."),svs.ssgiIntensity,svs.precision,0,100,1,false);
			AppendIn(propGISSGI,propGISSGIIntensity);

			propGISSGIRadius = new FloatProperty(_("Radius")+" (m)",_("Max distance of occluder to create occlusion."),svs.ssgiRadius,svs.precision,0,100,1,false);
			AppendIn(propGISSGI,propGISSGIRadius);

			propGISSGIAngleBias = new FloatProperty(_("Hide false edges"),_("0 for full occlusion (makes false edges visible), increase to hide occlusion on false edges."),svs.ssgiAngleBias,svs.precision,0,100,1,false);
			AppendIn(propGISSGI,propGISSGIAngleBias);
		}

		propGILDM = new BoolRefProperty(_("LDM"),_("Light detail maps improve quality of constant and realtime indirect illumination. LDMs are faster and look better than SSGI, but they have to be baked first (which takes time and requires unwrap)."),svs.renderLDM);
		AppendIn(propGIIndirect,propGILDM);

		// fireball
		{
			propGIFireball = new wxStringProperty(_("Fireball"), wxPG_LABEL);
			AppendIn(propGIIndirect,propGIFireball);
			SetPropertyReadOnly(propGIFireball,true,wxPG_DONT_RECURSE);

			propGIFireballQuality = new FloatProperty(_("Quality"),_("More = longer precalculation, higher quality realtime GI. Rebuild Fireball for this change to take effect."),svs.fireballQuality,0,0,1000000,100,false);
			AppendIn(propGIFireball,propGIFireballQuality);

			propGIFireballBuild = new ButtonProperty(_("Build"),_("Builds or rebuilds Fireball."),svframe,SVFrame::ME_REALTIME_FIREBALL_BUILD);
			AppendIn(propGIFireball,propGIFireballBuild);
			propGIFireballBuild->updateImage();

			propGIFireballWorkPerFrame = new wxIntProperty(_("Bounces in 1. frame"),wxPG_LABEL,svs.fireballWorkPerFrame);
			propGIFireballWorkPerFrame->SetHelpString(_("How much work Fireball does in first frame when GI needs update. Roughly equivalent to number of light bounces."));
			AppendIn(propGIFireball,propGIFireballWorkPerFrame);

			propGIFireballWorkTotal = new wxBoolProperty(_("Continue bouncing"),wxPG_LABEL,svs.fireballWorkTotal>svs.fireballWorkPerFrame);
			propGIFireballWorkTotal->SetHelpString(_("Uncheck to make Fireball stop all work after first frame. Check to see GI slowly improve over time."));
			SetPropertyEditor(propGIFireballWorkTotal,wxPGEditor_CheckBox);
			AppendIn(propGIFireball,propGIFireballWorkTotal);
		}

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

		SetPropertyBackgroundColour(propGIIndirect,importantPropertyBackgroundColor,false);
	}

	// cubes
	{
		propGIRaytracedCubes = new BoolRefProperty(_("Raytraced cubemaps"),_("Increases realism by realtime raytracing lowres or rasterizing hires cubemaps for diffuse and specular reflection and refraction. Note: Cubemaps automatically update only in realtime GI modes. Other modes preserve old cubemaps. You can manually update cubemaps by baking lightmaps or by switching to realtime GI and back."),svs.raytracedCubesEnabled);
		Append(propGIRaytracedCubes);
		
		propGIRaytracedCubesRes = new FloatProperty(_("Cube resolution"),_("Resolution of cube maps (total size is x*x*6 pixels). More = higher quality, slower. Default=16."),svs.raytracedCubesRes,0,1,1024,10,false);
		AppendIn(propGIRaytracedCubes,propGIRaytracedCubesRes);

		propGIRaytracedCubesMaxObjects = new FloatProperty(_("Max objects"),_("How many objects in scene before raytracing turns off automatically. Raytracing usually becomes bottleneck when there are more than 1000 objects."),svs.raytracedCubesMaxObjects,0,0,1000000,10,false);
		AppendIn(propGIRaytracedCubes,propGIRaytracedCubesMaxObjects);

		propGIRaytracedCubesSpecularThreshold = new FloatProperty(_("Specular threshold"),_("Only objects with specular color above threshold apply for specular cube reflection, 0=all objects apply, 1=only objects with spec color 1 apply."),svs.raytracedCubesSpecularThreshold,svs.precision,0,10,0.1f,false);
		AppendIn(propGIRaytracedCubesRes,propGIRaytracedCubesSpecularThreshold);

		propGIRaytracedCubesDepthThreshold = new FloatProperty(_("Depth threshold"),_("Only objects with depth above threshold apply for specular cube reflection, 0=all objects apply, 0.1=all but near planar objects apply, 1=none apply."),svs.raytracedCubesDepthThreshold,svs.precision,0,1,0.1f,false);
		AppendIn(propGIRaytracedCubesRes,propGIRaytracedCubesDepthThreshold);


		SetPropertyBackgroundColour(propGIRaytracedCubes,importantPropertyBackgroundColor,false);
	}

	// mirrors
	{
		propGIMirrors = new BoolRefProperty(_("Mirror reflections"),_("Increases realism by realtime rendering mirror reflections. Applied to flat meshes without cube reflections."),svs.mirrorsEnabled);
		Append(propGIMirrors);

		propGIMirrorsDiffuse = new BoolRefProperty(_("Diffuse"),_("Enables mirroring when rendering diffuse reflection."),svs.mirrorsDiffuse);
		AppendIn(propGIMirrors,propGIMirrorsDiffuse);

		propGIMirrorsSpecular = new BoolRefProperty(_("Specular"),_("Enables mirroring when rendering specular reflection."),svs.mirrorsSpecular);
		AppendIn(propGIMirrors,propGIMirrorsSpecular);

		propGIMirrorsQuality = new BoolRefProperty(_("Higher quality"),_("Improves quality by making reflection of close objects sharper."),svs.mirrorsMipmaps);
		AppendIn(propGIMirrors,propGIMirrorsQuality);

		SetPropertyBackgroundColour(propGIMirrors,importantPropertyBackgroundColor,false);

	}

	// lightmap
	{
		propGILightmap = new wxStringProperty(_("Baking"), wxPG_LABEL);
		Append(propGILightmap);
		SetPropertyReadOnly(propGILightmap,true,wxPG_DONT_RECURSE);

		propGILightmapFloats = new BoolRefProperty(_("HDR"),_("Renders lightmaps in floating point/high dynamic range format. Off = baked illumination is clamped to 0..1 range. HDR takes more memory, save it for scenes where LDR can't capture lighting well. Note that baking saves both LDR(.png) and HDR(.exr) maps to disk, this checkbox selects which ones to render."),svs.lightmapFloats);
		AppendIn(propGILightmap,propGILightmapFloats);

		propGILightmapAOIntensity = new FloatProperty(_("AO intensity"),_("Higher value makes indirect illumination in corners darker, 0=disabled/lighter, 1=normal, 2=darker."),svs.lightmapDirectParameters.aoIntensity,svs.precision,0,10,1,false);
		AppendIn(propGILightmap,propGILightmapAOIntensity);

		propGILightmapAOSize = new FloatProperty(_("AO size")+" (m)",_("Indirect illumination gets darker in this distance from corners, 0=disabled. If set too high, indirect illumination becomes completely black."),svs.lightmapDirectParameters.aoSize,svs.precision,0,1000,1,false);
		AppendIn(propGILightmap,propGILightmapAOSize);

		propGILightmapSmoothingAmount = new FloatProperty(_("Smoothing amount"),_("Amount of smoothing applied when baking lightmaps. Makes edges smoother, reduces noise, but washes out tiny details. Reasonable values are around 1. 0=off."),svs.lightmapFilteringParameters.smoothingAmount,svs.precision,0,10,1,false);
		AppendIn(propGILightmap,propGILightmapSmoothingAmount);

		propGILightmapSpreadForegroundColor = new wxIntProperty(_("Spread foreground"),wxPG_LABEL,svs.lightmapFilteringParameters.spreadForegroundColor);
		propGILightmapSpreadForegroundColor->SetHelpString(_("Affects invisible parts of lightmap only: How far in pixels do valid lightmap colors spread into adjacent unused pixels."));
		AppendIn(propGILightmap,propGILightmapSpreadForegroundColor);

		propGILightmapBackgroundColor = new HDRColorProperty(_("Background color"),_("Affects invisible parts of lightmap only: Color of unused pixels in lightmap."),svs.precision,svs.lightmapFilteringParameters.backgroundColor);
		AppendIn(propGILightmap,propGILightmapBackgroundColor);

		propGILightmapWrapping = new BoolRefProperty(_("Wrapping"),_("Checked = smoothing works across lightmap boundaries."),svs.lightmapFilteringParameters.wrap);
		AppendIn(propGILightmap,propGILightmapWrapping);

		propGIBuildLmaps = new ButtonProperty(_("Bake lightmaps"),_("Builds or rebuilds lightmaps (global illumination) for selected static objects."),svframe,CM_OBJECTS_BUILD_LMAPS);
		AppendIn(propGILightmap,propGIBuildLmaps);
		propGIBuildLmaps->updateImage();

		propGIBuildAmbientMaps = new ButtonProperty(_("Bake ambient maps"),_("Builds or rebuilds ambient maps (indirect illumination) for selected static objects."),svframe,CM_OBJECTS_BUILD_AMBIENT_MAPS);
		AppendIn(propGILightmap,propGIBuildAmbientMaps);
		propGIBuildAmbientMaps->updateImage();

		propGIBuildLDMs = new ButtonProperty(_("Build LDMs"),_("Builds or rebuilds LDMs for all selected static objects."),svframe,CM_OBJECTS_BUILD_LDMS);
		AppendIn(propGILightmap,propGIBuildLDMs);
		propGIBuildLDMs->updateImage();

		propGIBuildCubes = new ButtonProperty(_("Build cubemaps"),_("Builds or rebuilds cubemaps for selected objects."),svframe,CM_OBJECTS_BUILD_CUBES);
		AppendIn(propGILightmap,propGIBuildCubes);
		propGIBuildCubes->updateImage();
		propGIBilinear = new BoolRefProperty(_("Bilinear"),_("Bilinear interpolation of lightmaps and LDMs, keep always on, unless you analyze pixels."),svs.renderLightmapsBilinear);
		AppendIn(propGILightmap,propGIBilinear);

		SetPropertyBackgroundColour(propGILightmap,importantPropertyBackgroundColor,false);
	}

	updateHide();
}

//! Copy svs -> hide/show property.
//! Must not be called in every frame, float property that is unhid in every frame loses focus immediately after click, can't be edited.
void SVGIProperties::updateHide()
{
	propGIDirect->Hide(!svs.renderLightDirectRelevant(),false);
	propGISRGBCorrect->Hide(!svs.renderLightDirectActive(),false);
	propGIShadowTransparency->Hide(!svs.renderLightDirectActive(),false);

	propGIFireball->Hide(svs.renderLightIndirect!=LI_REALTIME_FIREBALL,false);

	bool realtimeGI = svs.renderLightIndirect==LI_REALTIME_FIREBALL || svs.renderLightIndirect==LI_REALTIME_ARCHITECT;
	propGISSGI->Hide(svs.renderLightIndirect==LI_PATHTRACED,false);
	propGISSGIIntensity->Hide(!svs.ssgiEnabled,false);
	propGISSGIRadius->Hide(!svs.ssgiEnabled,false);
	propGISSGIAngleBias->Hide(!svs.ssgiEnabled,false);
	propGILDM->Hide(!svs.renderLDMRelevant(),false);
	//propGIIndirectMultiplier->Hide(svs.renderLightIndirect==LI_NONE || svs.renderLightIndirect==LI_CONSTANT || svs.renderLightIndirect==LI_BAKED,false);
	//propGIEmisMultiplier->Hide(!realtimeGI,false);

	propGIRaytracedCubes->Hide(svs.renderLightIndirect==LI_PATHTRACED,false);
	propGIRaytracedCubesRes->Hide(!svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesMaxObjects->Hide(!svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesSpecularThreshold->Hide(!svs.raytracedCubesEnabled,false);
	propGIRaytracedCubesDepthThreshold->Hide(!svs.raytracedCubesEnabled,false);
	
	propGIMirrors->Hide(svs.renderLightIndirect==LI_PATHTRACED,false);
	propGIMirrorsDiffuse->Hide(!svs.mirrorsEnabled,false);
	propGIMirrorsSpecular->Hide(!svs.mirrorsEnabled,false);
	propGIMirrorsQuality->Hide(!svs.mirrorsEnabled,false);

	propGIVideo->Hide(!realtimeGI,false);
	propGIEmisVideoAffectsGI->Hide(!realtimeGI,false);
	propGIEmisVideoGIQuality->Hide(!realtimeGI || !svs.videoEmittanceAffectsGI,false);
	propGITranspVideoAffectsGI->Hide(!svs.renderLightDirectActive(),false);
	propGITranspVideoAffectsGIFull->Hide(!realtimeGI || !svs.videoTransmittanceAffectsGI,false);
	propGIEnvVideoAffectsGI->Hide(svs.renderLightIndirect!=LI_REALTIME_FIREBALL,false);
	propGIEnvVideoGIQuality->Hide(svs.renderLightIndirect!=LI_REALTIME_FIREBALL || !svs.videoEnvironmentAffectsGI,false);

	propGILightmap->Hide(svs.renderLightIndirect!=LI_LIGHTMAPS && svs.renderLightIndirect!=LI_AMBIENTMAPS,false);
}

void SVGIProperties::updateAfterGLInit()
{
	propGISRGBCorrect->Hide(!supportsSRGB(),false);
}

void SVGIProperties::updateProperties()
{
	// This function can update any property that has suddenly changed in svs, outside property grid.
	// We depend on it when new svs is loaded from disk.
	// Other approach would be to create all properties again, however,
	// this function would still have to support at least properties that user can change by hotkeys or mouse navigation.
	unsigned numChangesRelevantForHiding =
		+ updateBoolRef(propGIDirect)
		+ updateInt(propGIIndirect,svs.renderLightIndirect)
		+ updateBoolRef(propGISSGI)
		+ updateBoolRef(propGILDM)
		+ updateBoolRef(propGIEmisVideoAffectsGI)
		+ updateBoolRef(propGITranspVideoAffectsGI)
		+ updateBoolRef(propGIEnvVideoAffectsGI)
		+ updateBoolRef(propGIMirrors)
		;
	unsigned numChangesOther =
		+ updateBoolRef(propGISRGBCorrect)
		+ updateInt(propGIShadowTransparency,svs.shadowTransparency)
		+ updateFloat(propGISSGIIntensity,svs.ssgiIntensity)
		+ updateFloat(propGISSGIRadius,svs.ssgiRadius)
		+ updateFloat(propGISSGIAngleBias,svs.ssgiAngleBias)
		+ updateFloat(propGIIndirectMultiplier,svs.renderLightIndirectMultiplier)
		+ updateFloat(propGIEmisMultiplier,svs.emissiveMultiplier)
		+ updateInt(propGIFireballQuality,svs.fireballQuality)
		+ updateInt(propGIFireballWorkPerFrame,svs.fireballWorkPerFrame)
		+ updateBool(propGIFireballWorkTotal,svs.fireballWorkTotal>svs.fireballWorkPerFrame)
		+ updateBoolRef(propGIRaytracedCubes)
		+ updateInt(propGIRaytracedCubesRes,svs.raytracedCubesRes)
		+ updateInt(propGIRaytracedCubesMaxObjects,svs.raytracedCubesMaxObjects)
		+ updateFloat(propGIRaytracedCubesSpecularThreshold,svs.raytracedCubesSpecularThreshold)
		+ updateFloat(propGIRaytracedCubesDepthThreshold,svs.raytracedCubesDepthThreshold)
		+ updateInt(propGIEmisVideoGIQuality,svs.videoEmittanceGIQuality)
		+ updateBoolRef(propGITranspVideoAffectsGIFull)
		+ updateInt(propGIEnvVideoGIQuality,svs.videoEnvironmentGIQuality)
		+ updateBoolRef(propGIMirrorsDiffuse)
		+ updateBoolRef(propGIMirrorsSpecular)
		+ updateBoolRef(propGIMirrorsQuality)
		+ updateBoolRef(propGILightmapFloats)
		+ updateFloat(propGILightmapAOIntensity,svs.lightmapDirectParameters.aoIntensity)
		+ updateFloat(propGILightmapAOSize,svs.lightmapDirectParameters.aoSize)
		+ updateFloat(propGILightmapSmoothingAmount,svs.lightmapFilteringParameters.smoothingAmount)
		+ updateInt(propGILightmapSpreadForegroundColor,svs.lightmapFilteringParameters.spreadForegroundColor)
		+ updateProperty(propGILightmapBackgroundColor,rr::RRVec3(svs.lightmapFilteringParameters.backgroundColor))
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

void SVGIProperties::OnIdle(wxIdleEvent& event)
{
	if (IS_SHOWN(this))
	{
		updateProperties();
		defocusButtonEditor();
	}
}

void SVGIProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propGIDirect)
	{
		updateHide();
	}
	else
	if (property==propGIIndirect)
	{
		svs.renderLightIndirect = (LightingIndirect)property->GetValue().GetInteger();
		if (svs.renderLightIndirect==LI_REALTIME_FIREBALL)
			svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_FIREBALL);
		if (svs.renderLightIndirect==LI_REALTIME_ARCHITECT)
			svframe->OnMenuEventCore(SVFrame::ME_LIGHTING_INDIRECT_ARCHITECT);
		updateHide();
	}
	else
	if (property==propGIIndirectMultiplier)
	{
		svs.renderLightIndirectMultiplier = property->GetValue().GetDouble();
		if (svframe->m_canvas->solver)
		{
			// affects realtime solution only
			svframe->m_canvas->solver->setDirectIlluminationBoost(svs.renderLightIndirectMultiplier);
		}
	}
	else
	if (property==propGIEmisMultiplier)
	{
		svs.emissiveMultiplier = property->GetValue().GetDouble();
	}
	else
	if (property==propGISSGI)
	{
		updateHide();
	}
	else
	if (property==propGISSGIIntensity)
	{
		svs.ssgiIntensity = property->GetValue().GetDouble();
	}
	else
	if (property==propGISSGIRadius)
	{
		svs.ssgiRadius = property->GetValue().GetDouble();
	}
	else
	if (property==propGISSGIAngleBias)
	{
		svs.ssgiAngleBias = property->GetValue().GetDouble();
	}
	else
	if (property==propGILDM)
	{
		updateHide();
	}
	else
	if (property==propGIShadowTransparency)
	{
		svs.shadowTransparency = (rr_gl::RealtimeLight::ShadowTransparency)property->GetValue().GetInteger();
	}
	else
	if (property==propGIFireballQuality)
	{
		svs.fireballQuality = property->GetValue().GetInteger();
	}
	else
	if (property==propGIFireballWorkPerFrame)
	{
		svs.fireballWorkPerFrame = property->GetValue().GetInteger();
		if (svs.fireballWorkTotal<10000)
			svs.fireballWorkTotal = svs.fireballWorkPerFrame;
	}
	else
	if (property==propGIFireballWorkTotal)
	{
		svs.fireballWorkTotal = property->GetValue().GetBool() ? 10000 : svs.fireballWorkPerFrame;
	}
	else
	if (property==propGIRaytracedCubes)
	{
		updateHide();
		svframe->m_canvas->reallocateBuffersForRealtimeGI(false);
	}
	else
	if (property==propGIRaytracedCubesRes)
	{
		svs.raytracedCubesRes = propGIRaytracedCubesRes->GetValue().GetInteger();
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
	if (property==propGIMirrors)
	{
		updateHide();
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
	if (property==propGILightmapFloats)
	{
		// load selected set of maps from disk [#16]
		// (when user starts SV in LDR mode, only LDR maps are loaded if found, HDR are loaded after checking HDR here)
		rr::RRObjects allObjects = svframe->m_canvas->solver->getObjects();
		allObjects.layerDeleteFromMemory(svs.layerBakedLightmap);
		allObjects.loadLayer(svs.layerBakedLightmap,LAYER_PREFIX,LMAP_POSTFIX);
		allObjects.layerDeleteFromMemory(svs.layerBakedAmbient);
		allObjects.loadLayer(svs.layerBakedAmbient,LAYER_PREFIX,AMBIENT_POSTFIX);
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
	if (property==propGILightmapSpreadForegroundColor)
	{
		svs.lightmapFilteringParameters.spreadForegroundColor = property->GetValue().GetInteger();
	}
	else
	if (property==propGILightmapBackgroundColor)
	{
		svs.lightmapFilteringParameters.backgroundColor << property->GetValue();
	}
	else
	if (property==propGIBilinear)
	{
		for (unsigned i=0;i<svframe->m_canvas->solver->getStaticObjects().size();i++)
		{	
			unsigned layers[3] = {svs.layerBakedLightmap,svs.layerBakedAmbient,svs.layerBakedLDM};
			for (unsigned j=0;j<3;j++)
			{
				rr::RRBuffer* buf = svframe->m_canvas->solver->getStaticObjects()[i]->illumination.getLayer(layers[j]);
				if (buf && buf->getType()==rr::BT_2D_TEXTURE)
				{
					rr_gl::getTexture(buf)->bindTexture();
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, svs.renderLightmapsBilinear?GL_LINEAR:GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, svs.renderLightmapsBilinear?GL_LINEAR:GL_NEAREST);
				}
			}
		}
	}
	svframe->OnAnyChange(SVFrame::ES_PROPERTY,property);
}

BEGIN_EVENT_TABLE(SVGIProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVGIProperties::OnPropertyChange)
	EVT_IDLE(SVGIProperties::OnIdle)
END_EVENT_TABLE()

 
}; // namespace
