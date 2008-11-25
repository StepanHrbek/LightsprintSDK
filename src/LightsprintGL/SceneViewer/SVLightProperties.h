// --------------------------------------------------------------------------
// Scene viewer - light properties window.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVLIGHTPROPERTIES_H
#define SVLIGHTPROPERTIES_H

#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"
#include "Lightsprint/GL/RealtimeLight.h"

namespace rr_gl
{

	class SVLightProperties : public wxDialog
	{
	public:
		SVLightProperties( wxWindow* parent );

		//! Copy light -> property (all values, show/hide).
		void setLight(RealtimeLight* _rtlight);

		//! Copy light -> property (show/hide).
		void updateHide();

		//! Copy light -> property (8 floats).
		//! Update pos+dir properties according to current light state.
		//! They may be changed outside dialog, this must be called to update values in dialog.
		void updatePosDir();

		//! Copy light -> property (8 floats).
		void OnIdle(wxIdleEvent& event);

		//! Copy property -> light (all values).
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		RealtimeLight*    rtlight;
		wxPropertyGrid*   pg;
		wxPGProperty*     propType;
		wxPGProperty*     propPosition;
		wxPGProperty*     propDirection;
		wxPGProperty*     propOuterAngleRad;
		wxPGProperty*     propRadius;
		wxPGProperty*     propColor;
		wxPGProperty*     propTexture;
		wxPGProperty*     propDistanceAttType;
		wxPGProperty*     propConstant;
		wxPGProperty*     propLinear;
		wxPGProperty*     propQuadratic;
		wxPGProperty*     propClamp;
		wxPGProperty*     propFallOffExponent;
		wxPGProperty*     propSpotExponent;
		wxPGProperty*     propFallOffAngleRad;
		wxPGProperty*     propCastShadows;
		wxPGProperty*     propShadowmapRes;
		wxPGProperty*     propNear;
		wxPGProperty*     propFar;
		wxPGProperty*     propOrthoSize;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
