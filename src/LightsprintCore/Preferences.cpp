// --------------------------------------------------------------------------
// Persistent set of <string, float> pairs.
// Copyright (c) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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
		if (RegCreateKeyEx(hkey1, TEXT(subkey), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey2, &dwDisposition)==ERROR_SUCCESS)
		{
			DWORD dwType = REG_BINARY;
			DWORD dwSize = sizeof(value);
			RegQueryValueEx(hkey2, TEXT(variable), NULL, &dwType, (PBYTE)&value, &dwSize);
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
		if (RegCreateKeyEx(hkey1, TEXT(subkey), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey2, &dwDisposition)==ERROR_SUCCESS)
		{
			DWORD dwType = REG_BINARY;
			DWORD dwSize = sizeof(value);
			RegSetValueEx(hkey2, TEXT(variable), 0, dwType, (PBYTE)&value, dwSize);
			RegCloseKey(hkey2);
		}
		RegCloseKey(hkey1);
	}
#else
	RRReporter::report(WARN,"Preferences not implemented for this platform.\n");
#endif
}

} //namespace
