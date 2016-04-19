// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - light properties window.
// --------------------------------------------------------------------------

#ifndef SVLIGHTPROPERTIES_H
#define SVLIGHTPROPERTIES_H

#include "Lightsprint/Ed/Ed.h"
#include "SVProperties.h"
#include "SVCustomProperties.h"
#include "Lightsprint/GL/RealtimeLight.h"

namespace rr_ed
{

	class SVLightProperties : public SVProperties
	{
	public:
		SVLightProperties(SVFrame* svframe);

		//! Copy light -> property (all values, show/hide).
		void setLight(rr_gl::RealtimeLight* _rtlight, int _precision);
		rr_gl::RealtimeLight* getLight() {return rtlight;}

		//! Copy light -> property (show/hide).
		void updateHide();

		//! Copy light -> property (8 floats).
		//! Update pos+dir properties according to current light state.
		//! They may be changed outside dialog, this must be called to update values in dialog.
		void updatePosDir();

		//! Copy light -> property (8 floats), defocus.
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> light (all values).
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		rr_gl::RealtimeLight* rtlight;

		wxPGProperty*     propName;
		BoolRefProperty*  propEnabled;
		wxPGProperty*     propType;
		wxPGProperty*     propPosition;
		wxPGProperty*     propDirection;
		wxPGProperty*     propAltitude;
		wxPGProperty*     propAzimuth;
		wxPGProperty*     propOuterAngle;
		wxPGProperty*     propRadius;
		wxPGProperty*     propColor;
		ImageFileProperty*propTexture;
		wxPGProperty*     propTextureChangeAffectsGI;
		wxPGProperty*     propDistanceAttType;
		wxPGProperty*     propConstant;
		wxPGProperty*     propLinear;
		wxPGProperty*     propQuadratic;
		wxPGProperty*     propClamp;
		wxPGProperty*     propFallOffExponent;
		wxPGProperty*     propFallOffAngle;
		wxPGProperty*     propCastShadows;
		wxPGProperty*     propSpotExponent;
		wxPGProperty*     propShadowTransparency;
		wxPGProperty*     propShadowmaps;
		wxPGProperty*     propShadowmapRes;
		wxPGProperty*     propShadowSamples;
		wxPGProperty*     propShadowBias;
		BoolRefProperty*  propShadowAutomaticNearFar;
		wxPGProperty*     propNear;
		wxPGProperty*     propFar;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
