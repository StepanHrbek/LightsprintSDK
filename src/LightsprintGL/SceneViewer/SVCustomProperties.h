// --------------------------------------------------------------------------
// Scene viewer - custom properties for property grid.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVCUSTOMPROPERTIES_H
#define SVCUSTOMPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"
#include "wx/propgrid/advprops.h"


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
    RRVec3Property( const wxString& label = wxPG_LABEL,const wxString& name = wxPG_LABEL, int precision = -1, const RRVec3& value = RRVec3(0) );
    virtual void ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const;
    virtual void RefreshChildren();
};


//////////////////////////////////////////////////////////////////////////////
//
// HDRColorProperty

class HDRColorProperty : public wxPGProperty
{
    WX_PG_DECLARE_PROPERTY_CLASS(HDRColorProperty)
public:
    HDRColorProperty( const wxString& label = wxPG_LABEL, const wxString& name = wxPG_LABEL, int precision = -1, const RRVec3& value = RRVec3(1) );
    virtual void ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const;
    virtual void RefreshChildren();
};


//////////////////////////////////////////////////////////////////////////////
//
// RRBuffer* property

wxString getTextureDescription(rr::RRBuffer* buffer);
void setTextureFilename(rr::RRBuffer*& buffer, const wxPGProperty* filename, bool playVideos);


//////////////////////////////////////////////////////////////////////////////
//
// updates faster than plain SetValue

namespace rr_gl
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
