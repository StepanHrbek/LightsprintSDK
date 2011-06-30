// --------------------------------------------------------------------------
// Scene viewer - GI properties window.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVGIPROPERTIES_H
#define SVGIPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVProperties.h"
#include "SVCustomProperties.h"

namespace rr_gl
{

	class SVGIProperties : public SVProperties
	{
	public:
		SVGIProperties(SVFrame* svframe);

		//! Copy svs -> property.
		void updateProperties();

		//! Copy svs -> property, defocus.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> svs.
		void OnPropertyChange(wxPropertyGridEvent& event);

		//! Must be called after glewInit().
		void updateAfterGLInit();

	private:
		//! Copy svs -> hide/show property.
		void updateHide();

		wxPGProperty*     propGIDirect;
		wxPGProperty*     propGIIndirect;
		BoolRefProperty*  propGILDM;
		BoolRefProperty*  propGIBilinear;
		BoolRefProperty*  propGISRGBCorrect;
		wxPGProperty*     propGIShadowTransparency;
		wxPGProperty*     propGIFireballQuality;
		ButtonProperty*   propGIFireballBuild;
		BoolRefProperty*  propGIRaytracedCubes;
		wxPGProperty*     propGIRaytracedCubesDiffuseRes;
		wxPGProperty*     propGIRaytracedCubesSpecularRes;
		wxPGProperty*     propGIRaytracedCubesMaxObjects;
		FloatProperty*    propGIRaytracedCubesSpecularThreshold;
		FloatProperty*    propGIRaytracedCubesDepthThreshold;
		wxPGProperty*     propGIEmisMultiplier;
		wxPGProperty*     propGIVideo;
		BoolRefProperty*  propGIEmisVideoAffectsGI;
		wxPGProperty*     propGIEmisVideoGIQuality;
		BoolRefProperty*  propGITranspVideoAffectsGI;
		BoolRefProperty*  propGITranspVideoAffectsGIFull;
		BoolRefProperty*  propGIEnvVideoAffectsGI;
		wxPGProperty*     propGIEnvVideoGIQuality;
		wxPGProperty*     propGILightmap;
		wxPGProperty*     propGILightmapAOIntensity;
		wxPGProperty*     propGILightmapAOSize;
		wxPGProperty*     propGILightmapSmoothingAmount;
		BoolRefProperty*  propGILightmapWrapping;
		ButtonProperty*   propGIBuildLmaps;
		ButtonProperty*   propGIBuildLDMs;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif