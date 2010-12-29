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

	// import
	{
		propImport = new wxStringProperty(wxT("Import"), wxPG_LABEL);
		Append(propImport);
		SetPropertyReadOnly(propImport,true,wxPG_DONT_RECURSE);

		// units
		{
			const wxChar* viewStrings[] = {wxT("custom"),wxT("m"),wxT("inch"),wxT("cm"),wxT("mm"),NULL};
			const long viewValues[] = {ImportParameters::U_CUSTOM,ImportParameters::U_M,ImportParameters::U_INCH,ImportParameters::U_CM,ImportParameters::U_MM};
			propImportUnitsEnum = new wxEnumProperty(wxT("Units"), wxPG_LABEL, viewStrings, viewValues);
			propImportUnitsEnum->SetValueFromInt(userPreferences.import.unitEnum,wxPG_FULL_VALUE);
			propImportUnitsEnum->SetHelpString("How long it is if imported scene file says 1?");
			AppendIn(propImport,propImportUnitsEnum);

			propImportUnitsFloat = new FloatProperty(wxT("Unit length in meters"),"Define any other unit by specifying its lenth in meters.",userPreferences.import.unitFloat,svs.precision,0,10000000,1,false);
			AppendIn(propImportUnitsEnum,propImportUnitsFloat);

			propImportUnitsForce = new BoolRefProperty(wxT("Force units"),"Checked = selected units are used always. Unchecked = only if file does not know its own units.",userPreferences.import.unitForce);
			AppendIn(propImport,propImportUnitsForce);
		}

		// up
		{
			const wxChar* viewStrings[] = {wxT("x"),wxT("y"),wxT("z"),NULL};
			const long viewValues[] = {0,1,2};
			propImportUp = new wxEnumProperty(wxT("Up axis"), wxPG_LABEL, viewStrings, viewValues);
			propImportUp->SetValueFromInt(userPreferences.import.up,wxPG_FULL_VALUE);
			propImportUp->SetHelpString("What axis in imported scene file points up?");
			AppendIn(propImport,propImportUp);

			propImportUpForce = new BoolRefProperty(wxT("Force up"),"Checked = selected up is used always. Unchecked = only if file does not know its own up.",userPreferences.import.upForce);
			AppendIn(propImport,propImportUpForce);
		}

		SetPropertyBackgroundColour(propImport,headerColor,false);
	}

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
			propSshotEnhanced = new BoolRefProperty(wxT("Enhanced render"),"Enables enhanced (higher resolution and quality) screenshots. Enforces screenshot aspect in main viewport.",userPreferences.sshotEnhanced);
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
		svframe->m_canvas->OnSize(wxSizeEvent());
	}
	else
	if (property==propSshotEnhancedWidth)
	{
		userPreferences.sshotEnhancedWidth = property->GetValue().GetInteger();
		svframe->m_canvas->OnSize(wxSizeEvent());
	}
	else
	if (property==propSshotEnhancedHeight)
	{
		userPreferences.sshotEnhancedHeight = property->GetValue().GetInteger();
		svframe->m_canvas->OnSize(wxSizeEvent());
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
