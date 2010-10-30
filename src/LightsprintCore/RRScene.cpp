// --------------------------------------------------------------------------
// 3d scene and its import.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRScene.h"

namespace rr
{

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
	for (const char* src = extensionList; *src ; )
	{
		char* dst = extension;
		if (src[0]==';')
			src++;
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

struct LoaderExtensions
{
	RRScene::Loader* loader;
	RRString extensions; // "*.dae;*.3ds;*.md5mesh"
	// char* extensions would not work, some adapters register extensions using temporary string. RRString creates copy automatically
};

struct SaverExtensions
{
	RRScene::Saver* saver;
	RRString extensions; // "*.dae;*.3ds;*.md5mesh"
	// char* extensions would not work, some adapters register extensions using temporary string. RRString creates copy automatically
};

// static collection of registered loaders
std::vector<LoaderExtensions> s_loaders;

// static collection of registered savers
std::vector<SaverExtensions> s_savers;

#define S_EXTENSIONS_LEN 1000
char s_loaderExtensions[S_EXTENSIONS_LEN]; // "*.dae;*.3ds;*.md5mesh"
char s_saverExtensions[S_EXTENSIONS_LEN]; // "*.dae;*.3ds;*.md5mesh"


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

RRScene::RRScene(const char* filename, bool* aborting, float emissiveMultiplier)
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
			implementation = s_loaders[i].loader(filename,aborting,emissiveMultiplier);
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

bool RRScene::save(const char* filename)
{
	if (!filename)
	{
		return false;
	}

	RRReportInterval report(INF1,"Saving scene %s...\n",filename);

	// test whether savers were registered
	if (s_savers.empty())
	{
		RRReporter::report(WARN,"No savers registered, call rr_io::registerLoaders() or RRScene::registerSaver() first.\n");
		return false;
	}

	// attempt save
	bool saverFound = false;
	for (unsigned i=0;i<s_savers.size();i++)
	{
		if (extensionListMatches(filename,s_savers[i].extensions.c_str()))
		{
			if (s_savers[i].saver(this,filename))
				return true; // saved, success
			saverFound = true;
			// save failed, but don't give up for cycle yet,
			//  it's possible that another saver for the same extension will succeed
		}
	}

	// test whether saver exists
	if (!saverFound)
	{
		RRReporter::report(WARN,"Scene %s not saved, no saver for this extension was registered.\n",filename);
	}

	return false;
}

void RRScene::transform(const RRMatrix3x4& transformation)
{
	for (unsigned i=0;i<objects.size();i++)
	{
		RRObject* object = objects[i];
		if (object)
		{
			RRMatrix3x4 world = transformation*object->getWorldMatrixRef();
			object->setWorldMatrix(&world);
		}
	}
	RRReal scale = transformation.getUniformScale();
	for (unsigned i=0;i<lights.size();i++)
	{
		RRLight* light = lights[i];
		if (light)
		{
			transformation.transformPosition(light->position);
			light->direction = transformation.transformedDirection(light->direction).normalized();
			light->polynom[1] /= scale;
			light->polynom[2] /= scale*scale;
			light->radius *= scale;
		}
	}
}

void RRScene::normalizeUnits(float currentUnitLengthInMeters)
{
	RRMatrix3x4 m = {currentUnitLengthInMeters,0,0,0, 0,currentUnitLengthInMeters,0,0, 0,0,currentUnitLengthInMeters,0};
	transform(m);
}

void RRScene::normalizeUpAxis(unsigned currentUpAxis)
{
	switch (currentUpAxis)
	{
		case 0:
			{RRMatrix3x4 m = {0,-1,0,0, 1,0,0,0, 0,0,1,0}; transform(m);}
			return;
		case 1:
			return;
		case 2:
			{RRMatrix3x4 m = {1,0,0,0, 0,0,1,0, 0,-1,0,0}; transform(m);}
			return;
		default:
			RR_ASSERT(0);
			return;
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

		// update s_loaderExtensions
		if (s_loaders.size()==1)
			_snprintf(s_loaderExtensions,S_EXTENSIONS_LEN,"%s",extensions);
		else
			_snprintf(s_loaderExtensions+strlen(s_loaderExtensions),S_EXTENSIONS_LEN-strlen(s_loaderExtensions),";%s",extensions);
		s_loaderExtensions[S_EXTENSIONS_LEN-1] = 0;
	}
	else
	{
		RRReporter::report(WARN,"Invalid argument (NULL) in RRScene::registerLoader().\n");
	}
}

void RRScene::registerSaver(const char* extensions, Saver* saver)
{
	if (saver && extensions)
	{
		SaverExtensions se;
		se.saver = saver;
		se.extensions = extensions;
		s_savers.push_back(se);

		// update s_saverExtensions
		if (s_savers.size()==1)
			_snprintf(s_saverExtensions,S_EXTENSIONS_LEN,"%s",extensions);
		else
			_snprintf(s_saverExtensions+strlen(s_saverExtensions),S_EXTENSIONS_LEN-strlen(s_saverExtensions),";%s",extensions);
		s_saverExtensions[S_EXTENSIONS_LEN-1] = 0;
	}
	else
	{
		RRReporter::report(WARN,"Invalid argument (NULL) in RRScene::registerSaver().\n");
	}
}

const char* RRScene::getSupportedLoaderExtensions()
{
	if (s_loaders.empty())
	{
		// s_extensions not initialized yet
		return "";
	}
	return s_loaderExtensions;
}

const char* RRScene::getSupportedSaverExtensions()
{
	if (s_savers.empty())
	{
		// s_extensions not initialized yet
		return "";
	}
	return s_saverExtensions;
}

} // namespace rr
