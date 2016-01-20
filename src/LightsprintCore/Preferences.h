// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Persistent set of <string, float> pairs.
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
