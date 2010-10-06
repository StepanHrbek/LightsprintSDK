// --------------------------------------------------------------------------
// Scene viewer - user preferences window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVUserProperties.h"
#include "SVCustomProperties.h"

namespace rr_gl
{

SVUserProperties::SVUserProperties(SVFrame* _svframe)
	: SVProperties(_svframe), userPreferences(_svframe->userPreferences)
{
	wxColour headerColor(230,230,230);

	// sshot
	{
		propSshot = new wxStringProperty(wxT("Screenshots"), wxPG_LABEL);
		Append(propSshot);
		SetPropertyReadOnly(propSshot,true,wxPG_DONT_RECURSE);

		propSshotFilename = new wxFileProperty(wxT("Filename"),wxPG_LABEL,userPreferences.sshotFilename);
		propSshotFilename->SetHelpString("Filename, is automatically incremented on save.");
		AppendIn(propSshot,propSshotFilename);

		// enhanced
		{
			propSshotEnhanced = new BoolRefProperty(wxT("Enhanced render"),"Enables extra settings, won't save exactly what's on screen.",userPreferences.sshotEnhanced);
			AppendIn(propSshot,propSshotEnhanced);

			propSshotEnhancedWidth = new wxIntProperty(wxT("Width"),"Width in pixels, max depends on GPU.",userPreferences.sshotEnhancedWidth);
			AppendIn(propSshotEnhanced,propSshotEnhancedWidth);

			propSshotEnhancedHeight = new wxIntProperty(wxT("Height"),"Height in pixels, max depends on GPU.",userPreferences.sshotEnhancedHeight);
			AppendIn(propSshotEnhanced,propSshotEnhancedHeight);

			{
				const wxChar* viewStrings[] = {wxT("none"),wxT("4x"),wxT("8x"),wxT("16x"),NULL};
				const long viewValues[] = {1,4,9,16};
				propSshotEnhancedFSAA = new wxEnumProperty(wxT("FSAA"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedFSAA->SetValueFromInt(userPreferences.sshotEnhancedFSAA,wxPG_FULL_VALUE);
				propSshotEnhancedFSAA->SetHelpString("Fullscreen antialiasing mode, max depends on resolution and GPU.");
				AppendIn(propSshotEnhanced,propSshotEnhancedFSAA);
			}

			{
				const wxChar* viewStrings[] = {wxT("0.5x"),wxT("default"),wxT("2x"),wxT("4x"),NULL};
				const long viewValues[] = {1,2,4,8};
				propSshotEnhancedShadowResolutionFactor = new wxEnumProperty(wxT("Shadow resolution"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedShadowResolutionFactor->SetValueFromInt(userPreferences.sshotEnhancedShadowResolutionFactor*2,wxPG_FULL_VALUE);
				propSshotEnhancedShadowResolutionFactor->SetHelpString("Resolution of shadows, max depends on GPU and shadow resolution in lights.");
				AppendIn(propSshotEnhanced,propSshotEnhancedShadowResolutionFactor);
			}

			{
				const wxChar* viewStrings[] = {wxT("1"),wxT("2"),wxT("4"),wxT("default"),wxT("8"),NULL};
				const long viewValues[] = {1,2,4,0,8};
				propSshotEnhancedShadowSamples = new wxEnumProperty(wxT("Shadow samples"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedShadowSamples->SetValueFromInt(userPreferences.sshotEnhancedShadowSamples,wxPG_FULL_VALUE);
				propSshotEnhancedShadowSamples->SetHelpString("Number of shadow samples per pixel, max depends on GPU.");
				AppendIn(propSshotEnhanced,propSshotEnhancedShadowSamples);
			}
		}

		SetPropertyBackgroundColour(propSshot,headerColor,false);
	}

	updateHide();
}

void SVUserProperties::updateProperties()
{
	propSshotFilename->SetValue(userPreferences.sshotFilename);
}

void SVUserProperties::updateHide()
{
	propSshotEnhancedWidth->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedHeight->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedFSAA->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedShadowResolutionFactor->Hide(!userPreferences.sshotEnhanced,false);
	propSshotEnhancedShadowSamples->Hide(!userPreferences.sshotEnhanced,false);
}

void SVUserProperties::OnPropertyChange(wxPropertyGridEvent& event)
{
	wxPGProperty *property = event.GetProperty();

	if (property==propSshotFilename)
	{
		userPreferences.sshotFilename = property->GetValue().GetString();
	}
	else
	if (property==propSshotEnhanced)
	{
		updateHide();
	}
	else
	if (property==propSshotEnhancedWidth)
	{
		userPreferences.sshotEnhancedWidth = property->GetValue().GetInteger();
	}
	else
	if (property==propSshotEnhancedHeight)
	{
		userPreferences.sshotEnhancedHeight = property->GetValue().GetInteger();
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
}

BEGIN_EVENT_TABLE(SVUserProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVUserProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
