// --------------------------------------------------------------------------
// 3d scene and its import.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRScene.h"

namespace rr
{

struct LoaderExtension
{
	RRScene::Loader* loader;
	const char* extension;
};

// static collection of registered loaders
std::vector<LoaderExtension> s_loaders;

#define S_EXTENSIONS_LEN 200
char s_extensions[S_EXTENSIONS_LEN];

// case insensitive match
static bool extensionMatches(const char* filename, const char* extension)
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

/////////////////////////////////////////////////////////////////////////////
//
// RRScene

RRScene::RRScene(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
{
	implementation = NULL;
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
		if (extensionMatches(filename,s_loaders[i].extension))
		{
			loaderFound = true;
			implementation = s_loaders[i].loader(filename,scale,aborting,emissiveMultiplier);
			if (implementation)
			{
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
	delete implementation;
}

const RRObjects* RRScene::getObjects()
{
	return implementation ? implementation->getObjects() : NULL;
}

const RRLights* RRScene::getLights()
{
	return implementation ? implementation->getLights() : NULL;
}

const RRBuffer* RRScene::getEnvironment()
{
	return implementation ? implementation->getEnvironment() : NULL;
}

void RRScene::registerLoader(const char* extension, Loader* loader)
{
	if (loader && extension)
	{
		LoaderExtension le;
		le.loader = loader;
		le.extension = extension;
		s_loaders.push_back(le);

		// update s_extensions
		if (s_loaders.size()==1)
			_snprintf(s_extensions,S_EXTENSIONS_LEN,"*.%s",extension);
		else
			_snprintf(s_extensions+strlen(s_extensions),S_EXTENSIONS_LEN-strlen(s_extensions),";*.%s",extension);
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
