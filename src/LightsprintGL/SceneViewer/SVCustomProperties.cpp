// --------------------------------------------------------------------------
// Scene viewer - custom properties for property grid.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVCustomProperties.h"
#include "../tmpstr.h"


//////////////////////////////////////////////////////////////////////////////
//
// RRVec3Property

WX_PG_IMPLEMENT_VARIANT_DATA_DUMMY_EQ(RRVec3)

WX_PG_IMPLEMENT_PROPERTY_CLASS(RRVec3Property,wxPGProperty,RRVec3,const RRVec3&,TextCtrl)


RRVec3Property::RRVec3Property( const wxString& label, const wxString& name, int precision, const RRVec3& value )
    : wxPGProperty(label,name)
{
    SetValue( WXVARIANT(value) );
	wxPGProperty* prop;
	prop = new wxFloatProperty(wxT("x"),wxPG_LABEL,value.x);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(wxT("y"),wxPG_LABEL,value.y);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(wxT("z"),wxPG_LABEL,value.z);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
}

void RRVec3Property::RefreshChildren()
{
    if ( !GetChildCount() ) return;
    const RRVec3& vector = RRVec3RefFromVariant(m_value);
    Item(0)->SetValue( vector.x );
    Item(1)->SetValue( vector.y );
    Item(2)->SetValue( vector.z );
}

void RRVec3Property::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
{
    RRVec3 vector;
    vector << thisValue;
    switch ( childIndex )
    {
        case 0: vector.x = childValue.GetDouble(); break;
        case 1: vector.y = childValue.GetDouble(); break;
        case 2: vector.z = childValue.GetDouble(); break;
    }
    thisValue << vector;
}


//////////////////////////////////////////////////////////////////////////////
//
// HDRColorProperty

WX_PG_IMPLEMENT_PROPERTY_CLASS(HDRColorProperty,wxPGProperty,RRVec3,const RRVec3&,TextCtrl)

HDRColorProperty::HDRColorProperty( const wxString& label, const wxString& name, int precision, const RRVec3& rgb )
    : wxPGProperty(label,name)
{
    SetValue(WXVARIANT(rgb));

	RRVec3 hsv = rgb.getHsvFromRgb();
	wxPGProperty* prop;
	prop = new wxFloatProperty(_("red"),wxPG_LABEL,rgb[0]);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(_("green"),wxPG_LABEL,rgb[1]);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(_("blue"),wxPG_LABEL,rgb[2]);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(_("hue"),wxPG_LABEL,hsv[0]);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(_("saturation"),wxPG_LABEL,hsv[1]);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
	prop = new wxFloatProperty(_("value"),wxPG_LABEL,hsv[2]);
	prop->SetAttribute("Precision",precision);
    AddPrivateChild(prop);
}

void HDRColorProperty::RefreshChildren()
{
    if ( !GetChildCount() ) return;
	const RRVec3& rgb = RRVec3RefFromVariant(m_value);
	RRVec3 hsv = rgb.getHsvFromRgb();
    Item(0)->SetValue(rgb[0]);
    Item(1)->SetValue(rgb[1]);
    Item(2)->SetValue(rgb[2]);
    Item(3)->SetValue(hsv[0]);
    Item(4)->SetValue(hsv[1]);
    Item(5)->SetValue(hsv[2]);
}

void HDRColorProperty::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
{
    RRVec3 rgb;
    rgb << thisValue;
	RRVec3 hsv = rgb.getHsvFromRgb();
    switch ( childIndex )
    {
        case 0: 
        case 1: 
        case 2:
			rgb[childIndex] = childValue.GetDouble();
			break;
        case 3: 
        case 4: 
        case 5:
			hsv[childIndex-3] = childValue.GetDouble();
			rgb = hsv.getRgbFromHsv();
			break;
    }
    thisValue << rgb;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRBuffer* property

wxString getTextureDescription(rr::RRBuffer* buffer)
{
	return buffer
		? (buffer->filename.empty()
		?rr_gl::tmpstr("<%d*%d generated>",buffer->getWidth(),buffer->getHeight())
			:buffer->filename.c_str())
		:"<no texture>";
}

void setTextureFilename(rr::RRBuffer*& buffer, const wxPGProperty* filename, bool playVideos)
{
	if (buffer)
	{
		// stop if it looks like no more instances in scene
		// (one instance is us, second is in cache, third is in eventual Texture in customData)
		if (buffer->getReferenceCount()<=unsigned(buffer->customData?3:2))
			buffer->stop();
		delete buffer;
	}
	buffer = rr::RRBuffer::load(filename->GetValue().GetString().c_str());
}

#endif // SUPPORT_SCENEVIEWER
