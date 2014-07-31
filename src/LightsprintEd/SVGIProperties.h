// --------------------------------------------------------------------------
// Scene viewer - GI properties window.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVGIPROPERTIES_H
#define SVGIPROPERTIES_H

#include "Lightsprint/Ed/Ed.h"
#include "SVProperties.h"
#include "SVCustomProperties.h"

namespace rr_ed
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

	public:
		wxPGProperty*     propGITechnique;
	private:
		BoolRefProperty*  propGISRGBCorrect;
		BoolRefProperty*  propGIDirect;
		wxPGProperty*     propGIShadowTransparency;
		wxPGProperty*     propGISkyMultiplier;
		wxPGProperty*     propGIEmisMultiplier;
		wxPGProperty*     propGIIndirectMultiplier;
		BoolRefProperty*  propGISSGI;
		wxPGProperty*     propGISSGIIntensity;
		wxPGProperty*     propGISSGIRadius;
		wxPGProperty*     propGISSGIAngleBias;
		BoolRefProperty*  propGILDM;
		BoolRefProperty*  propGIBilinear;
		wxPGProperty*     propGIFireball;
		wxPGProperty*     propGIFireballQuality;
		ButtonProperty*   propGIFireballBuild;
		wxPGProperty*     propGIFireballWorkPerFrame;
		wxPGProperty*     propGIFireballWorkTotal;
		BoolRefProperty*  propGIRaytracedCubes;
		wxPGProperty*     propGIRaytracedCubesRes;
		wxPGProperty*     propGIRaytracedCubesMaxObjects;
		FloatProperty*    propGIRaytracedCubesSpecularThreshold;
		FloatProperty*    propGIRaytracedCubesDepthThreshold;
		BoolRefProperty*  propGIMirrors;
		BoolRefProperty*  propGIMirrorsDiffuse;
		BoolRefProperty*  propGIMirrorsSpecular;
		BoolRefProperty*  propGIMirrorsQuality;
		wxPGProperty*     propGIVideo;
		BoolRefProperty*  propGIEmisVideoAffectsGI;
		wxPGProperty*     propGIEmisVideoGIQuality;
		BoolRefProperty*  propGITranspVideoAffectsGI;
		BoolRefProperty*  propGITranspVideoAffectsGIFull;
		BoolRefProperty*  propGIEnvVideoAffectsGI;
		wxPGProperty*     propGIEnvVideoGIQuality;
		wxPGProperty*     propGILightmap;
		BoolRefProperty*  propGILightmapFloats;
		wxPGProperty*     propGILightmapAOIntensity;
		wxPGProperty*     propGILightmapAOSize;
		wxPGProperty*     propGILightmapSmoothingAmount;
		wxPGProperty*     propGILightmapSpreadForegroundColor;
		wxPGProperty*     propGILightmapBackgroundColor;
		BoolRefProperty*  propGILightmapWrapping;
		ButtonProperty*   propGIBuildLmaps;
		ButtonProperty*   propGIBuildAmbientMaps;
		ButtonProperty*   propGIBuildLDMs;
		ButtonProperty*   propGIBuildCubes;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
