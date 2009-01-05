// --------------------------------------------------------------------------
// Persistent set of <string, float> pairs.
// Copyright (c) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef PREFERENCES_H
#define PREFERENCES_H

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Preferences

//! Stores <string, float> pairs permanently. Implemented only for Windows.
class Preferences
{
public:
	static float getValue(const char* location, const char* variable, float defaultValue);
	static void setValue(const char* location, const char* variable, float value);
};

} //namespace

#endif
