// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Persistent set of <string, float> pairs.
// --------------------------------------------------------------------------

#include "Preferences.h"

#ifdef _WIN32
	#include <cstdio> // sprintf
	#include <windows.h>
#else
	#include "Lightsprint/RRDebug.h"
#endif

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Preferences

float Preferences::getValue(const char* location, const char* variable, float defaultValue)
{
#ifdef _WIN32
	float value = defaultValue;

	char subkey[200];
	_snprintf(subkey,199,"Software\\Lightsprint\\PM\\%s",location);
	subkey[199] = 0;

	HKEY hkey1;
	if (RegOpenCurrentUser(KEY_ALL_ACCESS,&hkey1)==ERROR_SUCCESS)
	{
		HKEY hkey2;
		DWORD dwDisposition;
		if (RegCreateKeyExA(hkey1, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hkey2, &dwDisposition)==ERROR_SUCCESS)
		{
			DWORD dwType = REG_BINARY;
			DWORD dwSize = sizeof(value);
			RegQueryValueExA(hkey2, variable, nullptr, &dwType, (PBYTE)&value, &dwSize);
			RegCloseKey(hkey2);
		}
		RegCloseKey(hkey1);
	}
	return value;
#else
	RRReporter::report(WARN,"Preferences not implemented for this platform.\n");
	return defaultValue;
#endif
}

void Preferences::setValue(const char* location, const char* variable, float value)
{
#ifdef _WIN32
	char subkey[200];
	_snprintf(subkey,199,"Software\\Lightsprint\\PM\\%s",location);
	subkey[199] = 0;

	HKEY hkey1;
	if (RegOpenCurrentUser(KEY_ALL_ACCESS,&hkey1)==ERROR_SUCCESS)
	{
		HKEY hkey2;
		DWORD dwDisposition;
		if (RegCreateKeyExA(hkey1, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hkey2, &dwDisposition)==ERROR_SUCCESS)
		{
			DWORD dwType = REG_BINARY;
			DWORD dwSize = sizeof(value);
			RegSetValueExA(hkey2, variable, 0, dwType, (PBYTE)&value, dwSize);
			RegCloseKey(hkey2);
		}
		RegCloseKey(hkey1);
	}
#else
	RRReporter::report(WARN,"Preferences not implemented for this platform.\n");
#endif
}

} //namespace
