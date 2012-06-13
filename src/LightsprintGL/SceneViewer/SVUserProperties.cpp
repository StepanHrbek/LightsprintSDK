// --------------------------------------------------------------------------
// Scene viewer - user preferences window.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVUserProperties.h"
#include "SVCustomProperties.h"


namespace rr_gl
{

SVUserProperties::SVUserProperties(SVFrame* _svframe)
	: SVProperties(_svframe), userPreferences(_svframe->userPreferences)
{

	// tooltips
	propTooltips = new BoolRefProperty(_("Tooltips"),_("Enables tooltips."),userPreferences.tooltips);
	Append(propTooltips);


	// stereo
	{
		{
			const wxChar* stereoStrings[] = {_("interlaced"),wxT("side by side"),wxT("top down"),NULL};
			const long stereoValues[] = {UserPreferences::SM_INTERLACED,UserPreferences::SM_SIDE_BY_SIDE,UserPreferences::SM_TOP_DOWN};
			propStereoMode = new wxEnumProperty(_("Stereo mode"), wxPG_LABEL, stereoStrings, stereoValues);
			propStereoMode->SetValueFromInt(userPreferences.stereoMode,wxPG_FULL_VALUE);
			propStereoMode->SetHelpString(_("How images for left and right eye are composited."));
			Append(propStereoMode);
		}

		propStereoSwap = new BoolRefProperty(_("Swap"),_("Swaps left and right eye. Should be checked if you see (with polarized glasses) the topmost line by left eye."),userPreferences.stereoSwap);
		AppendIn(propStereoMode,propStereoSwap);
	}

	// import
	{
		propImport = new wxStringProperty(_("Import"), wxPG_LABEL);
		Append(propImport);
		SetPropertyReadOnly(propImport,true,wxPG_DONT_RECURSE);

		// units
		{
			const wxChar* viewStrings[] = {_("custom"),wxT("m"),_("inch"),wxT("cm"),wxT("mm"),NULL};
			const long viewValues[] = {ImportParameters::U_CUSTOM,ImportParameters::U_M,ImportParameters::U_INCH,ImportParameters::U_CM,ImportParameters::U_MM};
			propImportUnitsEnum = new wxEnumProperty(_("Units"), wxPG_LABEL, viewStrings, viewValues);
			propImportUnitsEnum->SetValueFromInt(userPreferences.import.unitEnum,wxPG_FULL_VALUE);
			propImportUnitsEnum->SetHelpString(_("How long it is if imported scene file says 1?"));
			AppendIn(propImport,propImportUnitsEnum);

			propImportUnitsFloat = new FloatProperty(_("Unit length in meters"),_("Define any other unit by specifying its lenth in meters."),userPreferences.import.unitFloat,svs.precision,0,10000000,1,false);
			AppendIn(propImportUnitsEnum,propImportUnitsFloat);

			propImportUnitsForce = new BoolRefProperty(_("Force units"),_("Checked = selected units are used always. Unchecked = only if file does not know its own units."),userPreferences.import.unitForce);
			AppendIn(propImport,propImportUnitsForce);
		}

		// up
		{
			const wxChar* viewStrings[] = {wxT("x"),wxT("y"),wxT("z"),NULL};
			const long viewValues[] = {0,1,2};
			propImportUp = new wxEnumProperty(_("Up axis"), wxPG_LABEL, viewStrings, viewValues);
			propImportUp->SetValueFromInt(userPreferences.import.up,wxPG_FULL_VALUE);
			propImportUp->SetHelpString(_("What axis in imported scene file points up?"));
			AppendIn(propImport,propImportUp);

			propImportUpForce = new BoolRefProperty(_("Force up"),_("Checked = selected up is used always. Unchecked = only if file does not know its own up."),userPreferences.import.upForce);
			AppendIn(propImport,propImportUpForce);
		}

		SetPropertyBackgroundColour(propImport,importantPropertyBackgroundColor,false);
	}

	// sshot
	{
		propSshot = new wxStringProperty(_("Screenshots"), wxPG_LABEL);
		Append(propSshot);
		SetPropertyReadOnly(propSshot,true,wxPG_DONT_RECURSE);

		propSshotFilename = new wxFileProperty(_("Filename"),wxPG_LABEL,userPreferences.sshotFilename);
		propSshotFilename->SetHelpString(_("Filename, is automatically incremented on save."));
		AppendIn(propSshot,propSshotFilename);

		// enhanced
		{
			propSshotEnhanced = new BoolRefProperty(_("Enhanced render"),_("Enables enhanced (higher resolution and quality) screenshots. Enforces screenshot aspect in main viewport."),userPreferences.sshotEnhanced);
			AppendIn(propSshot,propSshotEnhanced);

			propSshotEnhancedWidth = new wxIntProperty(_("Width"),_("Width in pixels, max depends on GPU."),userPreferences.sshotEnhancedWidth);
			AppendIn(propSshotEnhanced,propSshotEnhancedWidth);

			propSshotEnhancedHeight = new wxIntProperty(_("Height"),_("Height in pixels, max depends on GPU."),userPreferences.sshotEnhancedHeight);
			AppendIn(propSshotEnhanced,propSshotEnhancedHeight);

			{
				const wxChar* viewStrings[] = {_("none"),wxT("4x"),wxT("8x"),wxT("16x"),NULL};
				const long viewValues[] = {1,4,9,16};
				propSshotEnhancedFSAA = new wxEnumProperty(_("Antialiasing"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedFSAA->SetValueFromInt(userPreferences.sshotEnhancedFSAA,wxPG_FULL_VALUE);
				propSshotEnhancedFSAA->SetHelpString(_("Fullscreen antialiasing mode, max depends on resolution and GPU."));
				AppendIn(propSshotEnhanced,propSshotEnhancedFSAA);
			}

			{
				const wxChar* viewStrings[] = {wxT("0.5x"),_("default"),wxT("2x"),wxT("4x"),NULL};
				const long viewValues[] = {1,2,4,8};
				propSshotEnhancedShadowResolutionFactor = new wxEnumProperty(_("Shadow resolution"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedShadowResolutionFactor->SetValueFromInt(userPreferences.sshotEnhancedShadowResolutionFactor*2,wxPG_FULL_VALUE);
				propSshotEnhancedShadowResolutionFactor->SetHelpString(_("Resolution of shadows, max depends on GPU and shadow resolution in lights."));
				AppendIn(propSshotEnhanced,propSshotEnhancedShadowResolutionFactor);
			}

			{
				const wxChar* viewStrings[] = {wxT("1"),wxT("2"),wxT("4"),_("default"),wxT("8"),NULL};
				const long viewValues[] = {1,2,4,0,8};
				propSshotEnhancedShadowSamples = new wxEnumProperty(_("Shadow samples"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedShadowSamples->SetValueFromInt(userPreferences.sshotEnhancedShadowSamples,wxPG_FULL_VALUE);
				propSshotEnhancedShadowSamples->SetHelpString(_("Number of shadow samples per pixel, max depends on GPU."));
				AppendIn(propSshotEnhanced,propSshotEnhancedShadowSamples);
			}

		}

		SetPropertyBackgroundColour(propSshot,importantPropertyBackgroundColor,false);
	}

	// testing
	{
		wxPGProperty* propTesting = new wxStringProperty(_("Testing"), wxPG_LABEL);
		Append(propTesting);
		SetPropertyReadOnly(propTesting,true,wxPG_DONT_RECURSE);

		propTestingLogShaders = new BoolRefProperty(_("Log shaders"),_("Logs more information, helps while debugging."),userPreferences.testingLogShaders);
		AppendIn(propTesting,propTestingLogShaders);

		propTestingLogMore = new BoolRefProperty(_("Log frame times"),_("Logs more information, helps while debugging."),userPreferences.testingLogMore);
		AppendIn(propTesting,propTestingLogMore);

		propTestingBeta = new BoolRefProperty(_("Beta functions"),_("Enables menu items with non-final features."),userPreferences.testingBeta);
		AppendIn(propTesting,propTestingBeta);

		SetPropertyBackgroundColour(propTesting,importantPropertyBackgroundColor,false);
	}

	updateHide();
}

void SVUserProperties::updateProperties()
{
	propSshotFilename->SetValue(userPreferences.sshotFilename);
}

void SVUserProperties::updateHide()
{
	propImportUnitsFloat->Hide(userPreferences.import.unitEnum!=ImportParameters::U_CUSTOM,false);
	propSshotEnhancedWidth->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedHeight->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedFSAA->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedShadowResolutionFactor->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedShadowSamples->Hide(!userPreferences.sshotEnhanced,false);
}

void SVUserProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propTooltips)
	{
		svframe->enableTooltips(svframe->userPreferences.tooltips);
	}
	else
	if (property==propStereoMode)
	{
		userPreferences.stereoMode = (UserPreferences::StereoMode)(property->GetValue().GetInteger());
		updateHide();
	}
	else
	if (property==propImportUnitsEnum)
	{
		userPreferences.import.unitEnum = (ImportParameters::Unit)(property->GetValue().GetInteger());
		updateHide();
	}
	else
	if (property==propImportUnitsFloat)
	{
		userPreferences.import.unitFloat = property->GetValue().GetDouble();
	}
	else
	if (property==propImportUp)
	{
		userPreferences.import.up = property->GetValue().GetInteger();
	}
	else
	if (property==propSshotFilename)
	{
		userPreferences.sshotFilename = property->GetValue().GetString();
	}
	else
	if (property==propSshotEnhanced)
	{
		updateHide();
		svframe->m_canvas->OnSizeCore(true);
	}
	else
	if (property==propSshotEnhancedWidth)
	{
		userPreferences.sshotEnhancedWidth = property->GetValue().GetInteger();
		svframe->m_canvas->OnSizeCore(true);
	}
	else
	if (property==propSshotEnhancedHeight)
	{
		userPreferences.sshotEnhancedHeight = property->GetValue().GetInteger();
		svframe->m_canvas->OnSizeCore(true);
	}
	else
	if (property==propSshotEnhancedFSAA)
	{
		userPreferences.sshotEnhancedFSAA = property->GetValue().GetInteger();
	}
	else
	if (property==propSshotEnhancedShadowResolutionFactor)
	{
		userPreferences.sshotEnhancedShadowResolutionFactor = property->GetValue().GetInteger()*0.5f;
	}
	else
	if (property==propSshotEnhancedShadowSamples)
	{
		userPreferences.sshotEnhancedShadowSamples = property->GetValue().GetInteger();
	}
	else
	if (property==propTestingLogShaders)
	{
		rr_gl::Program::logMessages(svframe->userPreferences.testingLogShaders);
	}
	else
	if (property==propTestingLogMore)
	{
		rr::RRReporter::setFilter(true,svframe->userPreferences.testingLogMore?3:2,true);
	}
}

BEGIN_EVENT_TABLE(SVUserProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVUserProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
