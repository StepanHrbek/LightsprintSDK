///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Apr 10 2012)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "Lightsprint/Ed/Ed.h"

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
	bSizer1->Add( m_button3, 0, int(wxALIGN_RIGHT) | int(wxALL), 5 );
	
	
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
	bSizer1->Add( m_button3, 0, int(wxALIGN_RIGHT) | int(wxALL), 5 );
	
	
	this->SetSizer( bSizer1 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

SmoothDlg::~SmoothDlg()
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
	
	lightmaps = new wxCheckBox( this, wxID_ANY, _("baked lightmaps"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( lightmaps, 0, wxALL, 5 );
	
	ambmaps = new wxCheckBox( this, wxID_ANY, _("baked ambient maps"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( ambmaps, 0, wxALL, 5 );
	
	envmaps = new wxCheckBox( this, wxID_ANY, _("baked environment maps"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( envmaps, 0, wxALL, 5 );
	
	ldms = new wxCheckBox( this, wxID_ANY, _("baked LDMs"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( ldms, 0, wxALL, 5 );
	
	animations = new wxCheckBox( this, wxID_ANY, _("animations"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( animations, 0, wxALL, 5 );
	
	m_button9 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 0,0 ), 0 );
	bSizer8->Add( m_button9, 0, wxALL, 5 );
	
	
	bSizer7->Add( bSizer8, 1, wxEXPAND, 5 );
	
	m_button7 = new wxButton( this, wxID_OK, _("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
	m_button7->SetDefault(); 
	bSizer7->Add( m_button7, 0, int(wxALIGN_RIGHT) | int(wxALL), 5 );
	
	
	this->SetSizer( bSizer7 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

DeleteDlg::~DeleteDlg()
{
}

BakeDlg::BakeDlg( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer12;
	bSizer12 = new wxBoxSizer( wxVERTICAL );
	
	wxGridSizer* gSizer2;
	gSizer2 = new wxGridSizer( 0, 2, 0, 0 );
	
	m_staticText17 = new wxStaticText( this, wxID_ANY, _("Quality"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText17->Wrap( -1 );
	gSizer2->Add( m_staticText17, 0, wxALL, 5 );
	
	wxString qualityChoices[] = { _("1"), _("10"), _("40"), _("100"), _("350"), _("1000"), _("3000"), _("10000"), _("30000"), _("100000") };
	int qualityNChoices = sizeof( qualityChoices ) / sizeof( wxString );
	quality = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, qualityNChoices, qualityChoices, 0 );
	quality->SetSelection( 3 );
	quality->SetToolTip( _("10=low, 100=medium, 1000=high, 10000=very high") );
	
	gSizer2->Add( quality, 0, wxALL, 5 );
	
	m_staticText18 = new wxStaticText( this, wxID_ANY, _("Resolution"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText18->Wrap( -1 );
	gSizer2->Add( m_staticText18, 0, wxALL, 5 );
	
	wxString resolutionChoices[] = { _("per-vertex"), _("8x8"), _("16x16"), _("32x32"), _("64x64"), _("128x128"), _("256x256"), _("512x512"), _("1024x1024"), _("2048x2048"), _("4096x4096") };
	int resolutionNChoices = sizeof( resolutionChoices ) / sizeof( wxString );
	resolution = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, resolutionNChoices, resolutionChoices, 0 );
	resolution->SetSelection( 6 );
	gSizer2->Add( resolution, 0, wxALL, 5 );
	
	useUnwrap = new wxCheckBox( this, wxID_ANY, _("Use unwrap resolution, if known"), wxDefaultPosition, wxDefaultSize, 0 );
	useUnwrap->SetValue(true); 
	useUnwrap->SetToolTip( _("Checked = objects with known Object panel / Mesh / Unwrap resolution use it instead of resolution selected above.") );
	
	gSizer2->Add( useUnwrap, 0, wxALL, 5 );
	
	m_button12 = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 0,0 ), 0 );
	gSizer2->Add( m_button12, 0, wxALL, 5 );
	
	m_staticText21 = new wxStaticText( this, wxID_ANY, _("      multiplied by"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText21->Wrap( -1 );
	gSizer2->Add( m_staticText21, 0, wxALL, 5 );
	
	multiplier = new wxTextCtrl( this, wxID_ANY, _("1"), wxDefaultPosition, wxDefaultSize, 0 );
	multiplier->SetToolTip( _("If we use unwrap resolution, we multiply it by this number. Sometimes you might want to bake at half resolution (0.5) without updating unwrap, just for quick preview. Or when baking Ambient maps, Lightmaps and LDMs for the same dataset, you might want to use different resolutions - ambient maps look well even with low resolutions, while LDMs need high resolutions. However, when baking at lower resolution, artifacts can show up. And when baking at higher resolutions, parts of texture space are wasted. For the best results, keep this multiplier within 1 to 4 range.") );
	
	gSizer2->Add( multiplier, 0, wxALL, 5 );
	
	
	bSizer12->Add( gSizer2, 1, wxEXPAND, 5 );
	
	button = new wxButton( this, wxID_OK, _("Bake"), wxDefaultPosition, wxDefaultSize, 0 );
	button->SetDefault(); 
	bSizer12->Add( button, 0, int(wxALIGN_RIGHT) | int(wxALL), 5 );
	
	
	this->SetSizer( bSizer12 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

BakeDlg::~BakeDlg()
{
}

UnwrapDlg::UnwrapDlg( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer8;
	bSizer8 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer9;
	bSizer9 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText7 = new wxStaticText( this, wxID_ANY, _("Unwrap resolution"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText7->Wrap( -1 );
	bSizer9->Add( m_staticText7, 0, wxALL, 5 );
	
	resolution = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	resolution->SetToolTip( _("Unwrap can be optimized for lightmaps of certain resolution. This is the resolution to optimize for, enter e.g. 256 for 256x256 lightmaps.") );
	
	bSizer9->Add( resolution, 0, wxALL, 5 );
	
	m_staticText8 = new wxStaticText( this, wxID_ANY, _("Number of triangles"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText8->Wrap( -1 );
	bSizer9->Add( m_staticText8, 0, wxALL, 5 );
	
	numTriangles = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	numTriangles->SetToolTip( _("Meshes with fewer triangles do high quality unwrap, meshes with more triangles do fast unwrap. If your unwraps take too much time, try reducing this number. Default is 25000.") );
	
	bSizer9->Add( numTriangles, 0, wxALL, 5 );
	
	cancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 0,0 ), 0 );
	bSizer9->Add( cancel, 0, wxALL, 5 );
	
	
	bSizer8->Add( bSizer9, 1, wxEXPAND, 5 );
	
	button = new wxButton( this, wxID_OK, _("Unwrap"), wxDefaultPosition, wxDefaultSize, 0 );
	button->SetDefault(); 
	bSizer8->Add( button, 0, int(wxALIGN_RIGHT) | int(wxALL), 5 );
	
	
	this->SetSizer( bSizer8 );
	this->Layout();
	
	this->Centre( wxBOTH );
}

UnwrapDlg::~UnwrapDlg()
{
}
