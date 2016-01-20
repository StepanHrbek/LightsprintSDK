// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - user preferences window.
// --------------------------------------------------------------------------

#ifndef SVUSERPROPERTIES_H
#define SVUSERPROPERTIES_H

#include "Lightsprint/Ed/Ed.h"
#include "SVProperties.h"

namespace rr_ed
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

		wxPGProperty*     propTooltips;

		wxPGProperty*     propSwapInterval;

		wxPGProperty*     propStereoMode;
		wxPGProperty*     propStereoSwap;

		wxPGProperty*     propImport;
		wxPGProperty*     propImportRemoveEmptyObjects;
		wxPGProperty*     propImportUnitsEnabled;
		wxPGProperty*     propImportUnitsEnum;
		wxPGProperty*     propImportUnitsFloat;
		wxPGProperty*     propImportUnitsForce;
		wxPGProperty*     propImportUpEnabled;
		wxPGProperty*     propImportUpEnum;
		wxPGProperty*     propImportUpForce;
		wxPGProperty*     propImportFlipFrontBackEnabled;
		wxPGProperty*     propImportFlipFrontBackEnum;
		wxPGProperty*     propImportTangents;


		wxPGProperty*     propSshot;
		wxPGProperty*     propSshotFilename;
		wxPGProperty*     propSshotEnhanced;
		wxPGProperty*     propSshotEnhancedWidth;
		wxPGProperty*     propSshotEnhancedHeight;
		wxPGProperty*     propSshotEnhancedFSAA;
		wxPGProperty*     propSshotEnhancedShadowResolutionFactor;
		wxPGProperty*     propSshotEnhancedShadowSamples;
		wxPGProperty*     propTestingLogShaders;
		wxPGProperty*     propTestingLogMore;
		wxPGProperty*     propTestingBeta;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif
