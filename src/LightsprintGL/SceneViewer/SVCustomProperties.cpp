// --------------------------------------------------------------------------
// Scene viewer - custom properties for property grid.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVCustomProperties.h"
#include "../tmpstr.h"


//////////////////////////////////////////////////////////////////////////////
//
// FloatProperty

FloatProperty::FloatProperty(const wxString& label, float value, int precision, float mini, float maxi, float step, bool wrap)
	: wxFloatProperty(label,wxPG_LABEL,value)
{
	step *= 0.1f;
	SetAttribute("Precision",precision);
	SetAttribute("Min",mini);
	SetAttribute("Max",maxi);
	SetAttribute("Step",step);
	SetAttribute("Wrap",wrap);
	SetAttribute("MotionSpin",true);
	SetEditor("SpinCtrl");
}


//////////////////////////////////////////////////////////////////////////////
//
// RRVec3Property

WX_PG_IMPLEMENT_VARIANT_DATA_DUMMY_EQ(RRVec3)

WX_PG_IMPLEMENT_PROPERTY_CLASS(RRVec3Property,wxPGProperty,RRVec3,const RRVec3&,TextCtrl)


RRVec3Property::RRVec3Property( const wxString& label, const wxString& name, int precision, const RRVec3& value, float step )
	: wxPGProperty(label,name)
{
	step *= 0.1f;
	SetValue( WXVARIANT(value) );
	AddPrivateChild(new FloatProperty("x",value.x,precision,-1e10f,1e10f,step,false));
	AddPrivateChild(new FloatProperty("y",value.y,precision,-1e10f,1e10f,step,false));
	AddPrivateChild(new FloatProperty("z",value.z,precision,-1e10f,1e10f,step,false));
}

void RRVec3Property::RefreshChildren()
{
	if ( !GetChildCount() ) return;
	const RRVec3& vector = RRVec3RefFromVariant(m_value);
	Item(0)->SetValue( vector.x );
	Item(1)->SetValue( vector.y );
	Item(2)->SetValue( vector.z );
}

wxVariant RRVec3Property::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
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
	return thisValue;
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
	AddPrivateChild(new FloatProperty("red",rgb[0],precision,0,100,0.1f,false));
	AddPrivateChild(new FloatProperty("green",rgb[1],precision,0,100,0.1f,false));
	AddPrivateChild(new FloatProperty("blue",rgb[2],precision,0,100,0.1f,false));
	AddPrivateChild(new FloatProperty("hue",hsv[0],precision,0,360,10,true));
	AddPrivateChild(new FloatProperty("saturation",hsv[1],precision,0,1,0.1f,false));
	AddPrivateChild(new FloatProperty("value",hsv[2],precision,0,100,0.1f,false));
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

wxVariant HDRColorProperty::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
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
	return thisValue;
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
	if (buffer && m_parentState) // be careful, wxWidgets SetValueImage would crash if m_parentState is NULL
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
	// must not contain <>*?/\, wxFileProperty would say "Validation conflict: '<no texture>' is invalid." on attempt to change texture
	return buffer
		? (buffer->filename.empty()
		?rr_gl::tmpstr("(%dx%d embedded)",buffer->getWidth(),buffer->getHeight())
			:buffer->filename.c_str())
		:"(no texture)";
}

#endif // SUPPORT_SCENEVIEWER
