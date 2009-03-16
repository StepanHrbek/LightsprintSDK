// --------------------------------------------------------------------------
// 3d scene and its import.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRScene.h"
#include <vector>

namespace rr
{

struct LoaderExtension
{
	RRScene::Loader* loader;
	const char* extension;
};

// static collection of registered loaders
std::vector<LoaderExtension> s_loaders;

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

RRScene::RRScene(const char* filename, float scale, bool stripPaths, bool* aborting, float emissiveMultiplier)
{
	rr::RRReportInterval report(rr::INF1,"Loading scene %s...\n",filename);

	implementation = NULL;

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
		RRReporter::report(WARN,"Scene %s does not exist.\n");
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
			implementation = s_loaders[i].loader(filename,scale,stripPaths,aborting,emissiveMultiplier);
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
		RRReporter::report(WARN,"Scene %s not loaded, no loader for this extension was registered.\n");
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

const rr::RRBuffer* RRScene::getEnvironment()
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
	}
	else
	{
		RRReporter::report(WARN,"Invalid argument (NULL) in RRScene::registerLoader().\n");
	}
}

} // namespace rr
