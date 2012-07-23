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
		wxTextCtrl* weldDistance;
		wxTextCtrl* uvDistance;
		wxCheckBox* splitVertices;
		wxCheckBox* stitchVertices;
		wxButton* m_button3;
		
		SmoothDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Smoothing options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 215,309 ), long style = wxCAPTION|wxCLOSE_BOX ); 
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
		wxCheckBox* cameras;
		wxCheckBox* lights;
		wxCheckBox* animations;
		wxCheckBox* environment;
		
		MergeDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select what to merge"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 200,224 ), long style = wxCAPTION|wxCLOSE_BOX ); 
		~MergeDlg();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DeleteDlg
///////////////////////////////////////////////////////////////////////////////
class DeleteDlg : public wxDialog 
{
	private:
	
	protected:
		wxButton* m_button7;
	
	public:
		wxCheckBox* unusedUvChannels;
		wxCheckBox* emptyFacegroups;
		wxCheckBox* tangents;
		wxCheckBox* unwrap;
		wxCheckBox* wholeObjects;
		
		DeleteDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Check what to delete"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 206,184 ), long style = wxDEFAULT_DIALOG_STYLE ); 
		~DeleteDlg();
	
};

#endif //__SVDIALOGS_H__
