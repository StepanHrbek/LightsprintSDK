///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "Lightsprint/GL/SceneViewer.h"

#include "SVDialogs.h"

///////////////////////////////////////////////////////////////////////////

ImportDlg::ImportDlg( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxVERTICAL );
	
	objects = new wxCheckBox( this, wxID_ANY, _("objects"), wxDefaultPosition, wxDefaultSize, 0 );
	objects->SetValue(true); 
	bSizer3->Add( objects, 0, wxALL, 5 );
	
	lights = new wxCheckBox( this, wxID_ANY, _("lights"), wxDefaultPosition, wxDefaultSize, 0 );
	lights->SetValue(true); 
	bSizer3->Add( lights, 0, wxALL, 5 );
	
	m_button4 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxPoint( -1,-1 ), wxSize( 0,0 ), 0 );
	bSizer3->Add( m_button4, 0, wxALL, 5 );
	
	bSizer1->Add( bSizer3, 1, wxEXPAND, 5 );
	
	m_button3 = new wxButton( this, wxID_OK, _("Import"), wxDefaultPosition, wxDefaultSize, 0 );
	m_button3->SetDefault(); 
	bSizer1->Add( m_button3, 0, wxALIGN_RIGHT|wxALL, 5 );
	
	this->SetSizer( bSizer1 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

ImportDlg::~ImportDlg()
{
}

SmoothDlg::SmoothDlg( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText3 = new wxStaticText( this, wxID_ANY, _("Apply to"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3->Wrap( -1 );
	bSizer3->Add( m_staticText3, 0, wxALL, 5 );
	
	wxString allMeshesChoices[] = { _("All meshes"), _("Selected mesh") };
	int allMeshesNChoices = sizeof( allMeshesChoices ) / sizeof( wxString );
	allMeshes = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, allMeshesNChoices, allMeshesChoices, 0 );
	allMeshes->SetSelection( 0 );
	bSizer3->Add( allMeshes, 0, wxALL, 5 );
	
	m_staticText1 = new wxStaticText( this, wxID_ANY, _("Smoothing angle (deg)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1->Wrap( -1 );
	bSizer3->Add( m_staticText1, 0, wxALL, 5 );
	
	smoothAngle = new wxTextCtrl( this, wxID_ANY, _("30"), wxDefaultPosition, wxDefaultSize, 0 );
	smoothAngle->SetToolTip( _("Max angle between faces to smooth edge.") );
	
	bSizer3->Add( smoothAngle, 0, wxALL, 5 );
	
	m_staticText2 = new wxStaticText( this, wxID_ANY, _("Stitching distance (m)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText2->Wrap( -1 );
	bSizer3->Add( m_staticText2, 0, wxALL, 5 );
	
	weldDistance = new wxTextCtrl( this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0 );
	weldDistance->SetToolTip( _("Max distance between vertices to stitch and smooth normals.") );
	
	bSizer3->Add( weldDistance, 0, wxALL, 5 );
	
	splitVertices = new wxCheckBox( this, wxID_ANY, _("Split vertices"), wxDefaultPosition, wxDefaultSize, 0 );
	splitVertices->SetValue(true); 
	bSizer3->Add( splitVertices, 0, wxALL, 5 );
	
	stitchVertices = new wxCheckBox( this, wxID_ANY, _("Stitch vertices"), wxDefaultPosition, wxDefaultSize, 0 );
	stitchVertices->SetValue(true); 
	bSizer3->Add( stitchVertices, 0, wxALL, 5 );
	
	preserveUvs = new wxCheckBox( this, wxID_ANY, _("Preserve texture mapping"), wxDefaultPosition, wxDefaultSize, 0 );
	preserveUvs->SetValue(true); 
	bSizer3->Add( preserveUvs, 0, wxALL, 5 );
	
	m_button4 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxPoint( -1,-1 ), wxSize( 0,0 ), 0 );
	bSizer3->Add( m_button4, 0, wxALL, 5 );
	
	bSizer1->Add( bSizer3, 1, wxEXPAND, 5 );
	
	m_button3 = new wxButton( this, wxID_OK, _("Smooth"), wxDefaultPosition, wxDefaultSize, 0 );
	m_button3->SetDefault(); 
	bSizer1->Add( m_button3, 0, wxALIGN_RIGHT|wxALL, 5 );
	
	this->SetSizer( bSizer1 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

SmoothDlg::~SmoothDlg()
{
}

MergeDlg::MergeDlg( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxVERTICAL );
	
	geometry = new wxCheckBox( this, wxID_ANY, _("geometry"), wxDefaultPosition, wxDefaultSize, 0 );
	geometry->SetValue(true); 
	bSizer3->Add( geometry, 0, wxALL, 5 );
	
	xrefs = new wxCheckBox( this, wxID_ANY, _("xrefs"), wxDefaultPosition, wxDefaultSize, 0 );
	xrefs->Enable( false );
	
	bSizer3->Add( xrefs, 0, wxALL, 5 );
	
	cameras = new wxCheckBox( this, wxID_ANY, _("cameras"), wxDefaultPosition, wxDefaultSize, 0 );
	cameras->SetValue(true); 
	bSizer3->Add( cameras, 0, wxALL, 5 );
	
	lights = new wxCheckBox( this, wxID_ANY, _("lights"), wxDefaultPosition, wxDefaultSize, 0 );
	lights->SetValue(true); 
	bSizer3->Add( lights, 0, wxALL, 5 );
	
	animations = new wxCheckBox( this, wxID_ANY, _("animations"), wxDefaultPosition, wxDefaultSize, 0 );
	animations->SetValue(true); 
	bSizer3->Add( animations, 0, wxALL, 5 );
	
	environment = new wxCheckBox( this, wxID_ANY, _("environment"), wxDefaultPosition, wxDefaultSize, 0 );
	environment->SetValue(true); 
	bSizer3->Add( environment, 0, wxALL, 5 );
	
	m_button4 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxPoint( -1,-1 ), wxSize( 0,0 ), 0 );
	bSizer3->Add( m_button4, 0, wxALL, 5 );
	
	bSizer1->Add( bSizer3, 1, wxEXPAND, 5 );
	
	m_button3 = new wxButton( this, wxID_OK, _("Merge"), wxDefaultPosition, wxDefaultSize, 0 );
	m_button3->SetDefault(); 
	bSizer1->Add( m_button3, 0, wxALIGN_RIGHT|wxALL, 5 );
	
	this->SetSizer( bSizer1 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

MergeDlg::~MergeDlg()
{
}
