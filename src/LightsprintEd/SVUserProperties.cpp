// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - user preferences window.
// --------------------------------------------------------------------------

#include "SVUserProperties.h"
#include "SVCustomProperties.h"


namespace rr_ed
{

SVUserProperties::SVUserProperties(SVFrame* _svframe)
	: SVProperties(_svframe), userPreferences(_svframe->userPreferences)
{

	// tooltips
	propTooltips = new BoolRefProperty(_("Tooltips"),_("Enables tooltips."),userPreferences.tooltips);
	Append(propTooltips);

	
	propSwapInterval = new wxBoolProperty(_("Max fps"),wxPG_LABEL,userPreferences.swapInterval==0);
	SetPropertyEditor(propSwapInterval,wxPGEditor_CheckBox);
	propSwapInterval->SetHelpString(_("Windows only. Checked = unrestricted fps, good only for benchmarking. Unchecked = fps adaptively synchronized with display refresh, smoother, reduced fan noise."));
	Append(propSwapInterval);

	// stereo
	{
		{
			const wxChar* stereoStrings[] = {_("interlaced"),_("side by side"),_("top down"),_("OpenVR (HTC Vive, Pimax 4k etc)"),_("Oculus Rift"),_("Quad Buffered"),nullptr};
			const long stereoValues[] = {rr::RRCamera::SM_INTERLACED,rr::RRCamera::SM_SIDE_BY_SIDE,rr::RRCamera::SM_TOP_DOWN,rr::RRCamera::SM_OPENVR,rr::RRCamera::SM_OCULUS_RIFT,rr::RRCamera::SM_QUAD_BUFFERED};
			propStereoMode = new wxEnumProperty(_("Stereo mode"), wxPG_LABEL, stereoStrings, stereoValues);
			propStereoMode->SetValueFromInt(userPreferences.stereoMode,wxPG_FULL_VALUE);
			propStereoMode->SetHelpString(_("Interlaced requires passive (polarized) display working in its native resolution. Quad buffered requires compatible GPU and display (consult GPU vendor). Quad buffered activates after application restart."));
			Append(propStereoMode);
		}

		propStereoSwap = new BoolRefProperty(_("Swap"),wxPG_LABEL,userPreferences.stereoSwap);
		propStereoSwap->SetHelpString(_("Swaps left and right eye."));
		SetPropertyEditor(propStereoSwap,wxPGEditor_CheckBox);
		AppendIn(propStereoMode,propStereoSwap);

		propStereoSupersampling = new FloatProperty("Supersampling",_("Multiplies number of pixels rendered, 1 for no change."),userPreferences.stereoSupersampling,svs.precision,0.1f,16,0.1f,false);
		AppendIn(propStereoMode,propStereoSupersampling);
	}

	// import
	{
		propImport = new wxStringProperty(_("Import"), wxPG_LABEL);
		Append(propImport);
		SetPropertyReadOnly(propImport,true,wxPG_DONT_RECURSE);

		propImportRemoveEmptyObjects = new BoolRefProperty(_("Remove empty"),_("Remove empty objects"),userPreferences.import.removeEmpty);
		AppendIn(propImport,propImportRemoveEmptyObjects);

		// units
		{
			propImportUnitsEnabled = new BoolRefProperty(_("Fix units"),_("Attempt to convert units to meters"),userPreferences.import.unitEnabled);
			AppendIn(propImport,propImportUnitsEnabled);

			const wxChar* viewStrings[] = {_("custom"),wxT("m"),_("inch"),wxT("cm"),wxT("mm"),nullptr};
			const long viewValues[] = {ImportParameters::U_CUSTOM,ImportParameters::U_M,ImportParameters::U_INCH,ImportParameters::U_CM,ImportParameters::U_MM};
			propImportUnitsEnum = new wxEnumProperty(_("Source units"), wxPG_LABEL, viewStrings, viewValues);
			propImportUnitsEnum->SetValueFromInt(userPreferences.import.unitEnum,wxPG_FULL_VALUE);
			propImportUnitsEnum->SetHelpString(_("How long it is if imported scene file says 1?"));
			AppendIn(propImportUnitsEnabled,propImportUnitsEnum);

			propImportUnitsFloat = new FloatProperty(_("Unit length in meters"),_("Define any other unit by specifying its lenth in meters."),userPreferences.import.unitFloat,svs.precision,0,10000000,1,false);
			AppendIn(propImportUnitsEnum,propImportUnitsFloat);

			propImportUnitsForce = new BoolRefProperty(_("Force"),_("Checked = selected units are used always. Unchecked = only if file does not know its own units."),userPreferences.import.unitForce);
			AppendIn(propImportUnitsEnabled,propImportUnitsForce);
		}

		// up
		{
			propImportUpEnabled = new BoolRefProperty(_("Fix coordinate system"),_("Attempt to orient axes in right directions"),userPreferences.import.upEnabled);
			AppendIn(propImport,propImportUpEnabled);

			const wxChar* viewStrings[] = {wxT("x"),wxT("y"),wxT("z"),nullptr};
			const long viewValues[] = {0,1,2};
			propImportUpEnum = new wxEnumProperty(_("Up axis"), wxPG_LABEL, viewStrings, viewValues);
			propImportUpEnum->SetValueFromInt(userPreferences.import.upEnum,wxPG_FULL_VALUE);
			propImportUpEnum->SetHelpString(_("What axis in imported scene file points up?"));
			AppendIn(propImportUpEnabled,propImportUpEnum);

			propImportUpForce = new BoolRefProperty(_("Force"),_("Checked = selected up is used always. Unchecked = only if file does not know its own up."),userPreferences.import.upForce);
			AppendIn(propImportUpEnabled,propImportUpForce);
		}

		// flipFrontBack
		{
			propImportFlipFrontBackEnabled = new BoolRefProperty(_("Fix front/back"),_("Attempt to fix front/back"),userPreferences.import.flipFrontBackEnabled);
			AppendIn(propImport,propImportFlipFrontBackEnabled);

			const wxChar* flipStrings[] = {_("never"),_("when 3 normals point back"),_("when 2 or 3 normals point back"),nullptr};
			const long flipValues[] = {4,3,2};
			propImportFlipFrontBackEnum = new wxEnumProperty(_("Flip front/back"), wxPG_LABEL, flipStrings, flipValues);
			propImportFlipFrontBackEnum->SetValueFromInt(userPreferences.import.flipFrontBackEnum,wxPG_FULL_VALUE);
			propImportFlipFrontBackEnum->SetHelpString(_("Changes order of vertices in some triangles (flips front/back). The idea is to make normals point to front side, it is important for correct illumination."));
			AppendIn(propImportFlipFrontBackEnabled,propImportFlipFrontBackEnum);
		}

		propImportTangents = new BoolRefProperty(_("Fix tangents"),_("Build missing tangents, fix broken ones, keep good ones."),userPreferences.import.tangentsEnabled);
		AppendIn(propImport,propImportTangents);

		SetPropertyBackgroundColour(propImport,importantPropertyBackgroundColor,false);
	}


	// sshot
	{
		propSshot = new wxStringProperty(_("Screenshots"), wxPG_LABEL);
		Append(propSshot);
		SetPropertyReadOnly(propSshot,true,wxPG_DONT_RECURSE);

		propSshotFilename = new wxFileProperty(_("Filename"),wxPG_LABEL,userPreferences.sshotFilename);
		propSshotFilename->SetHelpString(_("Filename (in current scene folder) or full path. It is automatically incremented on save."));
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
				const wxChar* viewStrings[] = {_("none"),wxT("4x"),wxT("8x"),wxT("16x"),nullptr};
				const long viewValues[] = {1,4,9,16};
				propSshotEnhancedFSAA = new wxEnumProperty(_("Antialiasing"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedFSAA->SetValueFromInt(userPreferences.sshotEnhancedFSAA,wxPG_FULL_VALUE);
				propSshotEnhancedFSAA->SetHelpString(_("Fullscreen antialiasing mode, max depends on resolution and GPU."));
				AppendIn(propSshotEnhanced,propSshotEnhancedFSAA);
			}

			{
				const wxChar* viewStrings[] = {wxT("0.5x"),_("default"),wxT("2x"),wxT("4x"),nullptr};
				const long viewValues[] = {1,2,4,8};
				propSshotEnhancedShadowResolutionFactor = new wxEnumProperty(_("Shadow resolution"), wxPG_LABEL, viewStrings, viewValues);
				propSshotEnhancedShadowResolutionFactor->SetValueFromInt(userPreferences.sshotEnhancedShadowResolutionFactor*2,wxPG_FULL_VALUE);
				propSshotEnhancedShadowResolutionFactor->SetHelpString(_("Resolution of shadows, max depends on GPU and shadow resolution in lights."));
				AppendIn(propSshotEnhanced,propSshotEnhancedShadowResolutionFactor);
			}

			{
				const wxChar* viewStrings[] = {wxT("1"),wxT("2"),wxT("4"),_("default"),wxT("8"),nullptr};
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
	propSwapInterval->SetValue(userPreferences.swapInterval==0);
	propStereoMode->SetValueFromInt(userPreferences.stereoMode,wxPG_FULL_VALUE);
}

void SVUserProperties::updateHide()
{
	//propStereoSupersampling->Hide(!userPreferences.stereo.mode!=VR,false);
	propImportUnitsEnum->Hide(!userPreferences.import.unitEnabled,false);
	propImportUnitsFloat->Hide(!userPreferences.import.unitEnabled || userPreferences.import.unitEnum!=ImportParameters::U_CUSTOM,false);
	propImportUnitsForce->Hide(!userPreferences.import.unitEnabled,false);
	propImportUpEnum->Hide(!userPreferences.import.upEnabled,false);
	propImportUpForce->Hide(!userPreferences.import.upEnabled,false);
	propImportFlipFrontBackEnum->Hide(!userPreferences.import.flipFrontBackEnabled,false);
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
	if (property==propSwapInterval)
	{
		userPreferences.swapInterval = property->GetValue().GetBool()?0:1;
		userPreferences.applySwapInterval();
	}
	else
	if (property==propStereoMode)
	{
		userPreferences.stereoMode = (rr::RRCamera::StereoMode)(propStereoMode->GetValue().GetInteger());
	}
	else
	if (property==propStereoSupersampling)
	{
		userPreferences.stereoSupersampling = property->GetValue().GetDouble();
	}
	else
	if (property==propImportUnitsEnabled)
	{
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
	if (property==propImportUpEnabled)
	{
		updateHide();
	}
	else
	if (property==propImportUpEnum)
	{
		userPreferences.import.upEnum = property->GetValue().GetInteger();
	}
	else
	if (property==propImportFlipFrontBackEnabled)
	{
		updateHide();
	}
	else
	if (property==propImportFlipFrontBackEnum)
	{
		userPreferences.import.flipFrontBackEnum = property->GetValue().GetInteger();
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
	svframe->OnAnyChange(SVFrame::ES_PROPERTY,property,nullptr,0);
}

BEGIN_EVENT_TABLE(SVUserProperties, wxPropertyGrid)
	EVT_PG_CHANGED(-1,SVUserProperties::OnPropertyChange)
END_EVENT_TABLE()

 
}; // namespace
