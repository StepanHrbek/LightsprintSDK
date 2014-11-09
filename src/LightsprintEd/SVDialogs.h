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
		wxCheckBox* lightmaps;
		wxCheckBox* ambmaps;
		wxCheckBox* envmaps;
		wxCheckBox* ldms;
		wxCheckBox* animations;
		
		DeleteDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Check what to delete"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 206,431 ), long style = wxDEFAULT_DIALOG_STYLE ); 
		~DeleteDlg();
	
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

///////////////////////////////////////////////////////////////////////////////
/// Class UnwrapDlg
///////////////////////////////////////////////////////////////////////////////
class UnwrapDlg : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_staticText7;
		wxStaticText* m_staticText8;
		wxButton* cancel;
		wxButton* button;
	
	public:
		wxTextCtrl* resolution;
		wxTextCtrl* numTriangles;
		
		UnwrapDlg( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Unwrapping options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 184,259 ), long style = wxDEFAULT_DIALOG_STYLE ); 
		~UnwrapDlg();
	
};

#endif //__SVDIALOGS_H__
