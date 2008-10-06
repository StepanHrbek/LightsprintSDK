// --------------------------------------------------------------------------
// Persistent set of <string, float> pairs.
// Copyright 2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
	HKEY   hkey;
	DWORD  dwDisposition;
 
	char subkey[200];
	_snprintf(subkey,199,"Software\\Lightsprint\\PM\\%s",location);
	subkey[199] = 0;

	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT(subkey), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition)==ERROR_SUCCESS)
	{
		DWORD dwType = REG_BINARY;
		DWORD dwSize = sizeof(value);
		RegQueryValueEx(hkey, TEXT(variable), NULL, &dwType, (PBYTE)&value, &dwSize);
		RegCloseKey(hkey);
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
	HKEY hkey;
	DWORD dwDisposition;
 
	char subkey[200];
	_snprintf(subkey,199,"Software\\Lightsprint\\PM\\%s",location);
	subkey[199] = 0;

	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT(subkey), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition)==ERROR_SUCCESS)
	{
		DWORD dwType = REG_BINARY;
		DWORD dwSize = sizeof(value);
		RegSetValueEx(hkey, TEXT(variable), 0, dwType, (PBYTE)&value, dwSize);
		RegCloseKey(hkey);
	}
#else
	RRReporter::report(WARN,"Preferences not implemented for this platform.\n");
#endif
}

} //namespace
