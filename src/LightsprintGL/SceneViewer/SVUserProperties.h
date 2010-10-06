// --------------------------------------------------------------------------
// Scene viewer - user preferences window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVUSERPROPERTIES_H
#define SVUSERPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVProperties.h"

namespace rr_gl
{

	class SVUserProperties : public SVProperties
	{
	public:
		SVUserProperties(SVFrame* svframe);

		//! Copy userPreferences -> property.
		void updateProperties();

		//! Copy property -> userPreferences.
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		//! Copy userPreferences -> hide/show property.
		void updateHide();

		UserPreferences&  userPreferences;

		wxPGProperty*     propSshot;
		wxPGProperty*     propSshotFilename;
		wxPGProperty*     propSshotEnhanced;
		wxPGProperty*     propSshotEnhancedWidth;
		wxPGProperty*     propSshotEnhancedHeight;
		wxPGProperty*     propSshotEnhancedFSAA;
		wxPGProperty*     propSshotEnhancedShadowResolutionFactor;
		wxPGProperty*     propSshotEnhancedShadowSamples;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
