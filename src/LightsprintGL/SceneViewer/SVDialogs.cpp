///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Apr 10 2012)
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
	
	m_staticText1 = new wxStaticText( this, wxID_ANY, _("Smoothing angle (deg)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1->Wrap( -1 );
	bSizer3->Add( m_staticText1, 0, wxALL, 5 );
	
	smoothAngle = new wxTextCtrl( this, wxID_ANY, _("30"), wxDefaultPosition, wxDefaultSize, 0 );
	smoothAngle->SetToolTip( _("Max angle between faces to merge vertices or stitch normals.") );
	
	bSizer3->Add( smoothAngle, 0, wxALL, 5 );
	
	m_staticText2 = new wxStaticText( this, wxID_ANY, _("Stitching distance (m)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText2->Wrap( -1 );
	bSizer3->Add( m_staticText2, 0, wxALL, 5 );
	
	stitchDistance = new wxTextCtrl( this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0 );
	stitchDistance->SetToolTip( _("Max distance between positions to merge vertices or stitch positions.") );
	
	bSizer3->Add( stitchDistance, 0, wxALL, 5 );
	
	m_staticText3 = new wxStaticText( this, wxID_ANY, _("UV distance"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3->Wrap( -1 );
	bSizer3->Add( m_staticText3, 0, wxALL, 5 );
	
	uvDistance = new wxTextCtrl( this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0 );
	uvDistance->SetToolTip( _("Max distance between uvs to merge vertices.") );
	
	bSizer3->Add( uvDistance, 0, wxALL, 5 );
	
	splitVertices = new wxCheckBox( this, wxID_ANY, _("Split vertices"), wxDefaultPosition, wxDefaultSize, 0 );
	splitVertices->SetValue(true); 
	splitVertices->SetToolTip( _("Splits vertices shared by multiple triangles (increases number of vertices), makes mesh less smooth.") );
	
	bSizer3->Add( splitVertices, 0, wxALL, 5 );
	
	mergeVertices = new wxCheckBox( this, wxID_ANY, _("Merge vertices"), wxDefaultPosition, wxDefaultSize, 0 );
	mergeVertices->SetValue(true); 
	mergeVertices->SetToolTip( _("Merges similar vertices (reduces number of vertices), makes mesh smoother.") );
	
	bSizer3->Add( mergeVertices, 0, wxALL, 5 );
	
	removeUnusedVertices = new wxCheckBox( this, wxID_ANY, _("Remove unused vertices"), wxDefaultPosition, wxDefaultSize, 0 );
	removeUnusedVertices->SetValue(true); 
	removeUnusedVertices->SetToolTip( _("Removes vertices not used in any triangle (reduces number of vertices).") );
	
	bSizer3->Add( removeUnusedVertices, 0, wxALL, 5 );
	
	removeDegeneratedTriangles = new wxCheckBox( this, wxID_ANY, _("Remove degenerated triangles"), wxDefaultPosition, wxDefaultSize, 0 );
	removeDegeneratedTriangles->SetValue(true); 
	removeDegeneratedTriangles->SetToolTip( _("Removes triangles with zero area (reduces number of triangles).") );
	
	bSizer3->Add( removeDegeneratedTriangles, 0, wxALL, 5 );
	
	stitchPositions = new wxCheckBox( this, wxID_ANY, _("Stitch positions"), wxDefaultPosition, wxDefaultSize, 0 );
	stitchPositions->SetValue(true); 
	stitchPositions->SetToolTip( _("Stitches positions of nearby vertices (doesn't change number of vertices).") );
	
	bSizer3->Add( stitchPositions, 0, wxALL, 5 );
	
	stitchNormals = new wxCheckBox( this, wxID_ANY, _("Stitch normals"), wxDefaultPosition, wxDefaultSize, 0 );
	stitchNormals->SetValue(true); 
	stitchNormals->SetToolTip( _("Stitches normals of nearby vertices (doesn't change number of vertices).") );
	
	bSizer3->Add( stitchNormals, 0, wxALL, 5 );
	
	generateNormals = new wxCheckBox( this, wxID_ANY, _("Generate new normals"), wxDefaultPosition, wxDefaultSize, 0 );
	generateNormals->SetValue(true); 
	generateNormals->SetToolTip( _("Generates new normals, completely ignoring old values.") );
	
	bSizer3->Add( generateNormals, 0, wxALL, 5 );
	
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
	
	materials = new wxCheckBox( this, wxID_ANY, _("materials"), wxDefaultPosition, wxDefaultSize, 0 );
	materials->SetValue(true); 
	bSizer3->Add( materials, 0, wxALL, 5 );
	
	lights = new wxCheckBox( this, wxID_ANY, _("lights"), wxDefaultPosition, wxDefaultSize, 0 );
	lights->SetValue(true); 
	bSizer3->Add( lights, 0, wxALL, 5 );
	
	cameras = new wxCheckBox( this, wxID_ANY, _("cameras"), wxDefaultPosition, wxDefaultSize, 0 );
	cameras->SetValue(true); 
	bSizer3->Add( cameras, 0, wxALL, 5 );
	
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

DeleteDlg::DeleteDlg( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer7;
	bSizer7 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer8;
	bSizer8 = new wxBoxSizer( wxVERTICAL );
	
	unusedUvChannels = new wxCheckBox( this, wxID_ANY, _("unused uv channels"), wxDefaultPosition, wxDefaultSize, 0 );
	unusedUvChannels->SetValue(true); 
	bSizer8->Add( unusedUvChannels, 0, wxALL, 5 );
	
	emptyFacegroups = new wxCheckBox( this, wxID_ANY, _("empty facegroups"), wxDefaultPosition, wxDefaultSize, 0 );
	emptyFacegroups->SetValue(true); 
	bSizer8->Add( emptyFacegroups, 0, wxALL, 5 );
	
	tangents = new wxCheckBox( this, wxID_ANY, _("tangents"), wxDefaultPosition, wxDefaultSize, 0 );
	tangents->SetValue(true); 
	bSizer8->Add( tangents, 0, wxALL, 5 );
	
	unwrap = new wxCheckBox( this, wxID_ANY, _("unwrap"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( unwrap, 0, wxALL, 5 );
	
	animations = new wxCheckBox( this, wxID_ANY, _("animations"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( animations, 0, wxALL, 5 );
	
	m_button9 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 0,0 ), 0 );
	bSizer8->Add( m_button9, 0, wxALL, 5 );
	
	
	bSizer7->Add( bSizer8, 1, wxEXPAND, 5 );
	
	m_button7 = new wxButton( this, wxID_OK, _("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
	m_button7->SetDefault(); 
	bSizer7->Add( m_button7, 0, wxALIGN_RIGHT|wxALL, 5 );
	
	
	this->SetSizer( bSizer7 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

DeleteDlg::~DeleteDlg()
{
}

MappingDialog::MappingDialog( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer9;
	bSizer9 = new wxBoxSizer( wxVERTICAL );
	
	wxGridSizer* gSizer1;
	gSizer1 = new wxGridSizer( 7, 3, 0, 0 );
	
	wxBoxSizer* bSizer10;
	bSizer10 = new wxBoxSizer( wxHORIZONTAL );
	
	radio_generate = new wxRadioButton( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	radio_generate->SetValue( true ); 
	radio_generate->SetToolTip( _("Generates new mapping, instead of using existing mapping.") );
	
	bSizer10->Add( radio_generate, 0, wxALL, 5 );
	
	m_staticText151 = new wxStaticText( this, wxID_ANY, _("Generate mapping"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText151->Wrap( -1 );
	bSizer10->Add( m_staticText151, 0, wxALL, 5 );
	
	
	gSizer1->Add( bSizer10, 1, wxEXPAND, 5 );
	
	generate = new wxComboBox( this, wxID_ANY, _("box"), wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
	generate->Append( _("box") );
	generate->Append( _("spherical") );
	generate->Append( _("cylindrical") );
	generate->Append( _("planar X") );
	generate->Append( _("planar Y") );
	generate->Append( _("planar Z") );
	generate->SetSelection( 0 );
	gSizer1->Add( generate, 0, wxALL, 5 );
	
	m_staticText23 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText23->Wrap( -1 );
	gSizer1->Add( m_staticText23, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer11;
	bSizer11 = new wxBoxSizer( wxHORIZONTAL );
	
	radio_source = new wxRadioButton( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	radio_source->SetToolTip( _("Takes mapping from existing uv channel, rather than generating it.") );
	
	bSizer11->Add( radio_source, 0, wxALL, 5 );
	
	m_staticText161 = new wxStaticText( this, wxID_ANY, _("Uv channel"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText161->Wrap( -1 );
	bSizer11->Add( m_staticText161, 0, wxALL, 5 );
	
	
	gSizer1->Add( bSizer11, 1, wxEXPAND, 5 );
	
	source = new wxSpinCtrl( this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS|wxSP_WRAP, 0, 7, 0 );
	gSizer1->Add( source, 0, wxALL, 5 );
	
	m_staticText24 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText24->Wrap( -1 );
	gSizer1->Add( m_staticText24, 0, wxALL, 5 );
	
	m_staticText7 = new wxStaticText( this, wxID_ANY, _("Transform"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText7->Wrap( -1 );
	gSizer1->Add( m_staticText7, 0, wxALL, 5 );
	
	m_staticText8 = new wxStaticText( this, wxID_ANY, _("scale"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText8->Wrap( -1 );
	gSizer1->Add( m_staticText8, 0, wxALL, 5 );
	
	scale = new wxTextCtrl( this, wxID_ANY, _("1"), wxDefaultPosition, wxDefaultSize, 0 );
	gSizer1->Add( scale, 0, wxALL, 5 );
	
	m_staticText10 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText10->Wrap( -1 );
	gSizer1->Add( m_staticText10, 0, wxALL, 5 );
	
	m_staticText11 = new wxStaticText( this, wxID_ANY, _("offset X"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText11->Wrap( -1 );
	gSizer1->Add( m_staticText11, 0, wxALL, 5 );
	
	offsetX = new wxTextCtrl( this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0 );
	gSizer1->Add( offsetX, 0, wxALL, 5 );
	
	m_staticText12 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText12->Wrap( -1 );
	gSizer1->Add( m_staticText12, 0, wxALL, 5 );
	
	m_staticText13 = new wxStaticText( this, wxID_ANY, _("offset Y"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText13->Wrap( -1 );
	gSizer1->Add( m_staticText13, 0, wxALL, 5 );
	
	offsetY = new wxTextCtrl( this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0 );
	gSizer1->Add( offsetY, 0, wxALL, 5 );
	
	m_staticText14 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText14->Wrap( -1 );
	gSizer1->Add( m_staticText14, 0, wxALL, 5 );
	
	m_staticText15 = new wxStaticText( this, wxID_ANY, _("angle (degrees)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText15->Wrap( -1 );
	gSizer1->Add( m_staticText15, 0, wxALL, 5 );
	
	angle = new wxTextCtrl( this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0 );
	gSizer1->Add( angle, 0, wxALL, 5 );
	
	m_staticText16 = new wxStaticText( this, wxID_ANY, _("Save uv channel"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText16->Wrap( -1 );
	gSizer1->Add( m_staticText16, 0, wxALL, 5 );
	
	destination = new wxSpinCtrl( this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS|wxSP_WRAP, 0, 7, 0 );
	gSizer1->Add( destination, 0, wxALL, 5 );
	
	m_button10 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 0,0 ), 0 );
	gSizer1->Add( m_button10, 0, wxALL, 5 );
	
	
	bSizer9->Add( gSizer1, 1, wxEXPAND, 5 );
	
	button = new wxButton( this, wxID_OK, _("Apply"), wxDefaultPosition, wxDefaultSize, 0 );
	button->SetDefault(); 
	bSizer9->Add( button, 0, wxALIGN_RIGHT|wxALL, 5 );
	
	
	this->SetSizer( bSizer9 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

MappingDialog::~MappingDialog()
{
}
