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
	: wxPGProperty(label,name), image(2,1), bitmap(NULL)
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

	// update ValueImage
	unsigned char* data = image.GetData();
	data[0] = data[3] = RR_FLOAT2BYTE(rgb[0]);
	data[1] = data[4] = RR_FLOAT2BYTE(rgb[1]);
	data[2] = data[5] = RR_FLOAT2BYTE(rgb[2]);
	delete bitmap;
	bitmap = new wxBitmap(image);
	SetValueImage(*bitmap);
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

HDRColorProperty::~HDRColorProperty()
{
	delete bitmap;
}


//////////////////////////////////////////////////////////////////////////////
//
// ImageFileProperty

ImageFileProperty::ImageFileProperty( const wxString& label )
	: wxFileProperty(label)
{
	image = NULL;
	bitmap = NULL;
}

ImageFileProperty::~ImageFileProperty()
{
	delete bitmap;
	delete image;
}

void ImageFileProperty::updateIcon(rr::RRBuffer* buffer)
{
	if (buffer)
	{
		wxSize size = wxSize(20,15);//GetImageSize();
		delete image;
		image = new wxImage(size);
		unsigned char* data = image->GetData();
		if (data)
		{
			for (int j=0;j<size.y;j++)
			for (int i=0;i<size.x;i++)
			{
				rr::RRVec4 rgba = buffer->getElement(rr::RRVec3((i+0.45f)/size.x,(j+0.45f)/size.y,0));
				data[(i+j*size.x)*3+0] = RR_FLOAT2BYTE(rgba[0]);
				data[(i+j*size.x)*3+1] = RR_FLOAT2BYTE(rgba[1]);
				data[(i+j*size.x)*3+2] = RR_FLOAT2BYTE(rgba[2]);
			}
		}
		delete bitmap;
		bitmap = new wxBitmap(*image);
		SetValueImage(*bitmap);
	}
	else
		SetValueImage(*(wxBitmap*)NULL);
}

void ImageFileProperty::updateBufferAndIcon(rr::RRBuffer*& buffer, bool playVideos)
{
	if (buffer && GetValue().GetString()!=buffer->filename.c_str())
	if (buffer)
	{
		// stop it
		buffer->stop();
		delete buffer;
	}
	buffer = rr::RRBuffer::load(GetValue().GetString().c_str());
	if (buffer && playVideos)
		buffer->play();

	updateIcon(buffer);
}

wxString getTextureDescription(rr::RRBuffer* buffer)
{
	return buffer
		? (buffer->filename.empty()
		?rr_gl::tmpstr("<%d*%d generated>",buffer->getWidth(),buffer->getHeight())
			:buffer->filename.c_str())
		:"<no texture>";
}

#endif // SUPPORT_SCENEVIEWER
