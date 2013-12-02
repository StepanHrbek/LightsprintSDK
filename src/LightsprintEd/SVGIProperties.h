// --------------------------------------------------------------------------
// Scene viewer - GI properties window.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
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

		wxPGProperty*     propGIDirect;
		wxPGProperty*     propGIIndirect;
		wxPGProperty*     propGIIndirectMultiplier;
		BoolRefProperty*  propGISSGI;
		wxPGProperty*     propGISSGIIntensity;
		wxPGProperty*     propGISSGIRadius;
		wxPGProperty*     propGISSGIAngleBias;
		BoolRefProperty*  propGILDM;
		BoolRefProperty*  propGIBilinear;
		BoolRefProperty*  propGISRGBCorrect;
		wxPGProperty*     propGIShadowTransparency;
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
		wxPGProperty*     propGIEmisMultiplier;
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

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
