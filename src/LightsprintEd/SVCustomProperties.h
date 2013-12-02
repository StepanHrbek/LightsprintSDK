// --------------------------------------------------------------------------
// Scene viewer - custom properties for property grid.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVCUSTOMPROPERTIES_H
#define SVCUSTOMPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/Ed/Ed.h"
#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"
#include "wx/propgrid/advprops.h"


//////////////////////////////////////////////////////////////////////////////
//
// FloatProperty

class FloatProperty : public wxFloatProperty
{
public:
	FloatProperty(const wxString& label, const wxString& help, float value, int precision, float mini, float maxi, float step, bool wrap);
	wxString ValueToString( wxVariant& value, int argFlags ) const;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRVec2Property

// wxVariant can handle only :: namespace classes
// workaround: we create RRVec2 copy and everything in ::
// this will be removed when possible
typedef rr::RRVec2 RRVec2; 

WX_PG_DECLARE_VARIANT_DATA(RRVec2)

class RRVec2Property : public wxPGProperty
{
	WX_PG_DECLARE_PROPERTY_CLASS(RRVec2Property)
public:
	RRVec2Property( const wxString& label = wxPG_LABEL,const wxString& name = wxPG_LABEL, int precision = -1, const RRVec2& value = RRVec2(0), float step = 1.f );
	virtual wxVariant ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const;
	virtual void RefreshChildren();
};


//////////////////////////////////////////////////////////////////////////////
//
// RRVec3Property

// wxVariant can handle only :: namespace classes
// workaround: we create RRVec3 copy and everything in ::
// this will be removed when possible
typedef rr::RRVec3 RRVec3; 

WX_PG_DECLARE_VARIANT_DATA(RRVec3)

class RRVec3Property : public wxPGProperty
{
	WX_PG_DECLARE_PROPERTY_CLASS(RRVec3Property)
public:
	RRVec3Property(const wxString& label = wxPG_LABEL,const wxString& help = wxPG_LABEL, int precision = -1, const RRVec3& value = RRVec3(0), float step = 1.f);
	virtual wxVariant ChildChanged(wxVariant& thisValue, int childIndex, wxVariant& childValue) const;
	virtual void RefreshChildren();
};


//////////////////////////////////////////////////////////////////////////////
//
// HDRColorProperty

class HDRColorProperty : public wxPGProperty
{
	WX_PG_DECLARE_PROPERTY_CLASS(HDRColorProperty)
public:
	HDRColorProperty(const wxString& label = wxPG_LABEL, const wxString& help = wxPG_LABEL, int precision = -1, const RRVec3& value = RRVec3(1));
	virtual ~HDRColorProperty();
	virtual wxVariant ChildChanged(wxVariant& thisValue, int childIndex, wxVariant& childValue) const;
	virtual void RefreshChildren();
	void updateImage();
	virtual wxString ValueToString(wxVariant& value, int argFlags) const;
	virtual bool StringToValue(wxVariant& variant, const wxString& text, int argFlags);
	virtual bool OnEvent(wxPropertyGrid *propgrid, wxWindow *wnd_primary, wxEvent &event);
private:
	wxImage image;
	wxBitmap* bitmap;
};


//////////////////////////////////////////////////////////////////////////////
//
// ButtonProperty

namespace rr_ed
{
	class SVFrame;
}

class ButtonProperty : public wxStringProperty // if we use wxPGProperty, value image is white
{
	WX_PG_DECLARE_PROPERTY_CLASS(ButtonProperty)
public:
	ButtonProperty(const wxString& label = wxPG_LABEL, const wxString& help = wxPG_LABEL, rr_ed::SVFrame* svframe = NULL, int menuItem = 0);
	void updateImage();
	virtual bool OnEvent(wxPropertyGrid *propgrid, wxWindow *wnd_primary, wxEvent &event);
private:
	int menuItem;
	rr_ed::SVFrame* svframe;
};


//////////////////////////////////////////////////////////////////////////////
//
// BoolRefProperty

class BoolRefProperty : public wxBoolProperty
{
public:
	BoolRefProperty(const wxString& label, const wxString& help, bool& value)
		: wxBoolProperty(label, wxPG_LABEL, value), ref(value)
	{
		SetValue(wxPGVariant_Bool(value));
		SetHelpString(help);
	}
	virtual void OnSetValue()
	{
		ref = GetValue().GetBool();
	}
	const wxPGEditor* DoGetEditorClass() const
	{
		return wxPGEditor_CheckBox;
	}
	bool& ref;
};


//////////////////////////////////////////////////////////////////////////////
//
// ImageFileProperty

class ImageFileProperty : public wxFileProperty
{
public:
	ImageFileProperty(const wxString& label, const wxString& help);
	virtual ~ImageFileProperty();
	void updateIcon(rr::RRBuffer* buffer);
	void updateBufferAndIcon(rr::RRBuffer*& buffer, bool playVideos);
private:
	wxImage* image;
	wxBitmap* bitmap;
};

wxString getTextureDescription(rr::RRBuffer* buffer);


//////////////////////////////////////////////////////////////////////////////
//
// FileComboProperty

// helper class, we need it to initialize filelist before base class (wxEnumProperty ) ctor gets called
class FileComboData
{
public:
	FileComboData(const wxString& dir);
protected:
	wxString fileDir;
	wxArrayString fileNames;
	wxArrayInt fileValues;
};

// dropdown menu with all files from given directory
class FileComboProperty : protected FileComboData, public wxEnumProperty
{
public:
	FileComboProperty(const wxString& label = wxPG_LABEL, const wxString& help = wxPG_LABEL, const wxString& dir = wxPG_LABEL);
	wxString getSelectedFullPath() const;
	bool contains(const wxString& filename) const;
};


//////////////////////////////////////////////////////////////////////////////
//
// LocationProperty

class LocationProperty : public wxPGProperty
{
	WX_PG_DECLARE_PROPERTY_CLASS(LocationProperty)
public:
	LocationProperty(const wxString& label = wxPG_LABEL, const wxString& help = wxPG_LABEL, int precision = -1, const RRVec2& latitudeLongitude = RRVec2(0));
	~LocationProperty();
	virtual wxVariant ChildChanged(wxVariant& thisValue, int childIndex, wxVariant& childValue) const;
	virtual void RefreshChildren();
private:
	const wxChar** cityStrings;
	long* cityValues;
};


//////////////////////////////////////////////////////////////////////////////
//
// updates faster than plain SetValue

namespace rr_ed
{

inline unsigned updateBool(wxPGProperty* prop,bool value)
{
	if (value!=prop->GetValue().GetBool())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
}

inline unsigned updateBoolRef(BoolRefProperty* prop)
{
	if (prop->ref!=prop->GetValue().GetBool())
	{
		prop->SetValue(wxPGVariant_Bool(prop->ref));
		return 1;
	}
	return 0;
}

inline unsigned updateInt(wxPGProperty* prop,int value)
{
	if (value!=(int)prop->GetValue().GetInteger())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
}

inline unsigned updateFloat(wxPGProperty* prop,float value)
{
	if (value!=(float)prop->GetValue().GetDouble())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
}

inline unsigned updateString(wxPGProperty* prop,wxString value)
{
	if (value!=prop->GetValue().GetString())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
}

inline unsigned updateDate(wxPGProperty* prop,wxDateTime value)
{
	if (value!=prop->GetValue().GetDateTime())
	{
		prop->SetValue(wxVariant(value));
		return 1;
	}
	return 0;
}

template <class C>
unsigned updateProperty(wxPGProperty* prop,C value)
{
	C oldValue;
	oldValue << prop->GetValue();
	if (value!=oldValue)
	{
		prop->SetValue(WXVARIANT(value));
		return 1;
	}
	return 0;
}

} // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
