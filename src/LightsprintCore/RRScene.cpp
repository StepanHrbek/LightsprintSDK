// --------------------------------------------------------------------------
// 3d scene and its import.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRScene.h"

namespace rr
{

struct LoaderExtensions
{
	RRScene::Loader* loader;
	RRString extensions; // "*.dae;*.3ds;*.md5mesh"
	// char* extensions would not work, some adapters register extensions using temporary string. RRString creates copy automatically
};

// static collection of registered loaders
std::vector<LoaderExtensions> s_loaders;

#define S_EXTENSIONS_LEN 1000
char s_extensions[S_EXTENSIONS_LEN]; // "*.dae;*.3ds;*.md5mesh"

// case insensitive match
static bool extensionMatches(const char* filename, const char* extension) // ext="3ds"
{
	if (!filename || !extension)
	{
		RR_ASSERT(0);
		return false;
	}
	size_t filelen = strlen(filename);
	size_t extlen = strlen(extension);
	if (filelen<1+extlen) return false; // filename too short
	if (_stricmp(filename+filelen-extlen,extension)) return false; // different extension 
	if (filename[filelen-extlen-1]!='.') return false; // extension not be preceded by '.'
	return true;
}

static bool extensionListMatches(const char* filename, const char* extensionList) // ext="*.3ds;*.mesh.xml"
{
	if (!filename || !extensionList)
	{
		RR_ASSERT(0);
		return false;
	}
	char extension[50];
	for (const char* src = extensionList; *src ; src++)
	{
		char* dst = extension;
		if (src[0]=='*' && src[1]=='.')
		{
			src += 2;
			if (*src=='*')
				return true; // *.* match
		}
		while (*src!=0 && *src!=';')
			*dst++ = *src++;
		*dst = 0;
		if (*extension && extensionMatches(filename,extension))
			return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
//
// RRScene

RRScene::RRScene()
{
	implementation = NULL;
	protectedObjects = NULL;
	protectedLights = NULL;
	environment = NULL;
}

RRScene::RRScene(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
{
	implementation = NULL;
	protectedObjects = NULL;
	protectedLights = NULL;
	environment = NULL;
	if (!filename)
	{
		// don't warn, it's documented as a valid way to create empty scene
		//RRReporter::report(WARN,"RRScene(NULL), invalid argument.\n");
		return;
	}

	RRReportInterval report(INF1,"Loading scene %s...\n",filename);

	// test whether loaders were registered
	if (s_loaders.empty())
	{
		RRReporter::report(WARN,"No loaders registered, call rr_io::registerLoaders() or RRScene::registerLoader() first.\n");
		return;
	}

	// test whether file exists (to properly report this common error)
	FILE* f = fopen(filename,"rb");
	if (!f)
	{
		RRReporter::report(WARN,"Scene %s does not exist.\n",filename);
		return;
	}
	else
	{
		fclose(f);
	}

	// attempt load
	bool loaderFound = false;
	for (unsigned i=0;i<s_loaders.size();i++)
	{
		if (extensionListMatches(filename,s_loaders[i].extensions.c_str()))
		{
			loaderFound = true;
			implementation = s_loaders[i].loader(filename,scale,aborting,emissiveMultiplier);
			if (implementation)
			{
				lights = implementation->protectedLights ? *implementation->protectedLights : implementation->lights;
				objects = implementation->protectedObjects ? *implementation->protectedObjects : implementation->objects;
				if (implementation->environment)
					environment = implementation->environment->createReference();
				return; // loaded, success
			}
			// load failed, but don't give up for cycle yet,
			//  it's possible that another loader for the same extension will succeed
		}
	}

	// test whether loader exists
	if (!loaderFound)
	{
		RRReporter::report(WARN,"Scene %s not loaded, no loader for this extension was registered.\n",filename);
		return;
	}

	if (!implementation)
	{
		// exact reason was already reported by loader
		//RRReporter::report(WARN,"Scene %s load failed.\n");
	}
}

RRScene::~RRScene()
{
	delete environment;
	delete protectedLights;
	delete protectedObjects;
	delete implementation;
}

void RRScene::registerLoader(const char* extensions, Loader* loader)
{
	if (loader && extensions)
	{
		LoaderExtensions le;
		le.loader = loader;
		le.extensions = extensions;
		s_loaders.push_back(le);

		// update s_extensions
		if (s_loaders.size()==1)
			_snprintf(s_extensions,S_EXTENSIONS_LEN,extensions);
		else
			_snprintf(s_extensions+strlen(s_extensions),S_EXTENSIONS_LEN-strlen(s_extensions),";%s",extensions);
		s_extensions[S_EXTENSIONS_LEN-1] = 0;
	}
	else
	{
		RRReporter::report(WARN,"Invalid argument (NULL) in RRScene::registerLoader().\n");
	}
}

const char* RRScene::getSupportedExtensions()
{
	if (s_loaders.empty())
	{
		// s_extensions not initialized yet
		return "";
	}
	return s_extensions;
}

} // namespace rr
