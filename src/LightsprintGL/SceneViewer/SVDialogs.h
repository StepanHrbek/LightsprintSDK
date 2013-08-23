///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Apr 10 2012)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __SVDIALOGS_H__
#define __SVDIALOGS_H__

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/checkbox.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/radiobut.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class ImportDlg
///////////////////////////////////////////////////////////////////////////////
class ImportDlg : public wxDialog 
{
	private:
	
	protected:
		wxButton* m_button4;
		wxButton* m_button3;
	
	public:
		wxCheckBox* objects;
		wxCheckBox* lights;
		
		ImportDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select what to import"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 253,150 ), long style = wxCAPTION|wxCLOSE_BOX ); 
		~ImportDlg();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class SmoothDlg
///////////////////////////////////////////////////////////////////////////////
class SmoothDlg : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_staticText1;
		wxStaticText* m_staticText2;
		wxStaticText* m_staticText3;
		wxButton* m_button4;
	
	public:
		wxTextCtrl* smoothAngle;
		wxTextCtrl* stitchDistance;
		wxTextCtrl* uvDistance;
		wxCheckBox* splitVertices;
		wxCheckBox* mergeVertices;
		wxCheckBox* removeUnusedVertices;
		wxCheckBox* removeDegeneratedTriangles;
		wxCheckBox* stitchPositions;
		wxCheckBox* stitchNormals;
		wxCheckBox* generateNormals;
		wxButton* m_button3;
		
		SmoothDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Smoothing options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 215,573 ), long style = wxCAPTION|wxCLOSE_BOX ); 
		~SmoothDlg();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class MergeDlg
///////////////////////////////////////////////////////////////////////////////
class MergeDlg : public wxDialog 
{
	private:
	
	protected:
		wxCheckBox* xrefs;
		wxButton* m_button4;
		wxButton* m_button3;
	
	public:
		wxCheckBox* geometry;
		wxCheckBox* materials;
		wxCheckBox* lights;
		wxCheckBox* cameras;
		wxCheckBox* animations;
		wxCheckBox* environment;
		
		MergeDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select what to merge"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 200,299 ), long style = wxCAPTION|wxCLOSE_BOX ); 
		~MergeDlg();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DeleteDlg
///////////////////////////////////////////////////////////////////////////////
class DeleteDlg : public wxDialog 
{
	private:
	
	protected:
		wxButton* m_button9;
		wxButton* m_button7;
	
	public:
		wxCheckBox* unusedUvChannels;
		wxCheckBox* emptyFacegroups;
		wxCheckBox* tangents;
		wxCheckBox* unwrap;
		wxCheckBox* animations;
		
		DeleteDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Check what to delete"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 206,250 ), long style = wxDEFAULT_DIALOG_STYLE ); 
		~DeleteDlg();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class MappingDialog
///////////////////////////////////////////////////////////////////////////////
class MappingDialog : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_staticText151;
		wxStaticText* m_staticText23;
		wxStaticText* m_staticText161;
		wxStaticText* m_staticText24;
		wxStaticText* m_staticText7;
		wxStaticText* m_staticText8;
		wxStaticText* m_staticText10;
		wxStaticText* m_staticText11;
		wxStaticText* m_staticText12;
		wxStaticText* m_staticText13;
		wxStaticText* m_staticText14;
		wxStaticText* m_staticText15;
		wxStaticText* m_staticText16;
		wxButton* m_button10;
	
	public:
		wxRadioButton* radio_generate;
		wxComboBox* generate;
		wxRadioButton* radio_source;
		wxSpinCtrl* source;
		wxTextCtrl* scale;
		wxTextCtrl* offsetX;
		wxTextCtrl* offsetY;
		wxTextCtrl* angle;
		wxSpinCtrl* destination;
		wxButton* button;
		
		MappingDialog( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Generate or modify uv mapping"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 366,330 ), long style = wxDEFAULT_DIALOG_STYLE ); 
		~MappingDialog();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class BakeDlg
///////////////////////////////////////////////////////////////////////////////
class BakeDlg : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_staticText17;
		wxStaticText* m_staticText18;
		wxButton* m_button12;
		wxStaticText* m_staticText21;
	
	public:
		wxChoice* quality;
		wxChoice* resolution;
		wxCheckBox* useUnwrap;
		wxTextCtrl* multiplier;
		wxButton* button;
		
		BakeDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 254,233 ), long style = wxCAPTION|wxCLOSE_BOX ); 
		~BakeDlg();
	
};

#endif //__SVDIALOGS_H__
