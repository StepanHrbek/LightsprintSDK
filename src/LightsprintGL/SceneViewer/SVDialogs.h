///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __SVDialogs__
#define __SVDialogs__

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
		
		ImportDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select what to import"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 253,119 ), long style = wxCAPTION|wxCLOSE_BOX );
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
		wxButton* m_button4;
		wxButton* m_button3;
	
	public:
		wxTextCtrl* smoothAngle;
		wxTextCtrl* weldDistance;
		wxCheckBox* splitVertices;
		wxCheckBox* stitchVertices;
		wxCheckBox* preserveUvs;
		
		SmoothDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Smoothing options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 215,243 ), long style = wxCAPTION|wxCLOSE_BOX );
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
		
		MergeDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select what to merge"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 215,205 ), long style = wxCAPTION|wxCLOSE_BOX );
		~MergeDlg();
	
};

#endif //__SVDialogs__
