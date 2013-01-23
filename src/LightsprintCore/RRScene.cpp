// --------------------------------------------------------------------------
// 3d scene and its import.
// Copyright (c) 2006-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRScene.h"
#ifdef _MSC_VER
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif
#include <boost/filesystem.hpp>

namespace bf = boost::filesystem;

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// generic loaders/savers

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

static bool extensionListMatches(const RRString& filename, const char* extensionList) // ext="*.3ds;*.mesh.xml"
{
	if (!extensionList)
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
		if (src[0]==':' && (src[1]==';' || src[1]==0)) // [#14] : is our symbol that matches everything, but unlike *.*, fileselector does not understand it
		{
			src += 1;
			return true; // : matches all, like *.*
		}
		while (*src!=0 && *src!=';')
			*dst++ = *src++;
		*dst = 0;
		if (*extension && extensionMatches(filename.c_str(),extension))
			return true;
	}
	return false;
}

template <class RRClass>
struct LoaderExtensions
{
	typename RRClass::Loader* loader;
	RRString extensions; // "*.dae;*.3ds;*.md5mesh"
	// char* extensions would not work, some adapters register extensions using temporary string. RRString creates copy automatically
};

template <class RRClass>
struct SaverExtensions
{
	typename RRClass::Saver* saver;
	RRString extensions; // "*.dae;*.3ds;*.md5mesh"
	// char* extensions would not work, some adapters register extensions using temporary string. RRString creates copy automatically
};

template <class RRClass>
struct LoadersAndSavers
{
	// collection of registered loaders
	std::vector<LoaderExtensions<RRClass> > loaders;

	// collection of registered savers
	std::vector<SaverExtensions<RRClass> > savers;

	#define S_EXTENSIONS_LEN 1000
	char loaderExtensions[S_EXTENSIONS_LEN]; // "*.dae;*.3ds;*.md5mesh"
	char saverExtensions[S_EXTENSIONS_LEN]; // "*.dae;*.3ds;*.md5mesh"

	void registerLoader(const char* extensions, typename RRClass::Loader* loader)
	{
		if (loader && extensions)
		{
			LoaderExtensions<RRClass> le;
			le.loader = loader;
			le.extensions = extensions;
			loaders.push_back(le);

			// update s_loaderExtensions
			if (loaders.size()==1)
				_snprintf(loaderExtensions,S_EXTENSIONS_LEN,"%s",extensions);
			else
				_snprintf(loaderExtensions+strlen(loaderExtensions),S_EXTENSIONS_LEN-strlen(loaderExtensions),";%s",extensions);
			loaderExtensions[S_EXTENSIONS_LEN-1] = 0;
		}
		else
		{
			RRReporter::report(WARN,"Invalid argument (NULL) in registerLoader().\n");
		}
	}

	void registerSaver(const char* extensions, typename RRClass::Saver* saver)
	{
		if (saver && extensions)
		{
			SaverExtensions<RRClass> se;
			se.saver = saver;
			se.extensions = extensions;
			savers.push_back(se);

			// update s_saverExtensions
			if (savers.size()==1)
				_snprintf(saverExtensions,S_EXTENSIONS_LEN,"%s",extensions);
			else
				_snprintf(saverExtensions+strlen(saverExtensions),S_EXTENSIONS_LEN-strlen(saverExtensions),";%s",extensions);
			saverExtensions[S_EXTENSIONS_LEN-1] = 0;
		}
		else
		{
			RRReporter::report(WARN,"Invalid argument (NULL) in registerSaver().\n");
		}
	}

	const char* getSupportedLoaderExtensions()
	{
		return loaders.empty() ? "" : loaderExtensions;
	}

	const char* getSupportedSaverExtensions()
	{
		return savers.empty() ? "" : saverExtensions;
	}

	static RRClass* callLoader(typename RRClass::Loader* _loader, const RRString& _filename, RRFileLocator* _locator, bool* _aborting, const char* classname)
	{
#ifdef _MSC_VER
		// for example .rr3 import crashes on bogus .rr3 files, it is catched here
		__try
		{
#endif
			return _loader(_filename,_locator,_aborting);
#ifdef _MSC_VER
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"%s import crashed.\n",classname));
			return false;
		}
#endif
	}

	static bool callSaver(typename RRClass::Saver* _saver, const RRClass* _instance, const RRString& _filename, const char* classname)
	{
#ifdef _MSC_VER
		__try
		{
#endif
			return _saver(_instance,_filename);
#ifdef _MSC_VER
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"%s export crashed.\n",classname));
			return false;
		}
#endif
	}

	RRClass* load(const RRString& _filename, RRFileLocator* _textureLocator, bool* _aborting, const char* classname)
	{
		if (_filename.empty())
		{
			// don't warn, it's documented as a valid way to create empty scene
			//RRReporter::report(WARN,"RRScene(NULL), invalid argument.\n");
			return NULL;
		}

		RRReportInterval report(INF1,"Loading %s %ls...\n",classname,_filename.w_str());

		// test whether loaders were registered
		if (loaders.empty())
		{
			RRReporter::report(WARN,"No loaders registered, call rr_io::registerLoaders() or RR%s::registerLoader() first.\n",classname);
			return false;
		}

		// test whether file exists (to properly report this common error)
		boost::system::error_code ec;
		if (!bf::exists(RR_RR2PATH(_filename),ec))
		{
			RRReporter::report(WARN,"%ls does not exist.\n",_filename.w_str());
			return false;
		}

		// tell texture locator scene filename
		RRFileLocator* localTextureLocator = _textureLocator?_textureLocator:RRFileLocator::create();
		localTextureLocator->setParent(true,_filename);

		// attempt load
		bool loaderFound = false;
		RRClass* loaded = NULL;
		for (unsigned i=0;i<loaders.size();i++)
		{
			if (extensionListMatches(_filename,loaders[i].extensions.c_str()))
			{
				loaderFound = true;
				try
				{
					loaded = callLoader(loaders[i].loader,_filename,localTextureLocator,_aborting,classname);
				}
				catch (...)
				{
					RRReporter::report(WARN,"Import ended by throwing C++ exception, something is broken.\n");
				}
				if (loaded)
				{
					break; // loaded, success
				}
				// load failed, but don't give up for cycle yet,
				//  it's possible that another loader for the same extension will succeed
			}
		}

		// cleanup
		localTextureLocator->setParent(false,_filename);
		if (!_textureLocator)
			delete localTextureLocator;

		// report result
		if (!loaderFound)
		{
			RRReporter::report(WARN,"%ls not loaded, no loader for this extension was registered.\n",_filename.w_str());
		}
		else
		if (!loaded)
		{
			// exact reason was already reported by loader
			//RRReporter::report(WARN,"%s load failed.\n");
		}
		return loaded;
	}

	bool save(const RRClass* _instance, const RRString& _filename, const char* classname)
	{
		if (!_instance || _filename.empty())
		{
			return false;
		}

		RRReportInterval report(INF1,"Saving %s %ls...\n",classname,_filename.w_str());

		// test whether savers were registered
		if (savers.empty())
		{
			RRReporter::report(WARN,"No savers registered, call rr_io::registerLoaders() or RR%s::registerSaver() first.\n",classname);
			return false;
		}

		// attempt save
		bool saverFound = false;
		for (unsigned i=0;i<savers.size();i++)
		{
			if (extensionListMatches(_filename,savers[i].extensions.c_str()))
			{
				try
				{
					if (callSaver(savers[i].saver,_instance,_filename,classname))
						return true; // saved, success
				}
				catch (...)
				{
					RRReporter::report(WARN,"Export ended by throwing C++ exception, something is broken.\n");
				}
				saverFound = true;
				// save failed, but don't give up for cycle yet,
				//  it's possible that another saver for the same extension will succeed
			}
		}

		// test whether saver exists
		if (!saverFound)
		{
			RRReporter::report(WARN,"%ls not saved, no saver for this extension was registered.\n",_filename.w_str());
		}

		return false;
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// RRScene loaders/savers

static LoadersAndSavers<RRScene> s_sceneLoadersAndSavers;

void RRScene::registerLoader(const char* extensions, Loader* loader)
{
	s_sceneLoadersAndSavers.registerLoader(extensions,loader);
}

void RRScene::registerSaver(const char* extensions, Saver* saver)
{
	s_sceneLoadersAndSavers.registerSaver(extensions,saver);
}

const char* RRScene::getSupportedLoaderExtensions()
{
	return s_sceneLoadersAndSavers.getSupportedLoaderExtensions();
}

const char* RRScene::getSupportedSaverExtensions()
{
	return s_sceneLoadersAndSavers.getSupportedSaverExtensions();
}

RRScene::RRScene(const RRString& _filename, RRFileLocator* _textureLocator, bool* _aborting)
{
	protectedObjects = NULL;
	protectedLights = NULL;
	environment = NULL;
	implementation = s_sceneLoadersAndSavers.load(_filename,_textureLocator,_aborting,"Scene");
	if (implementation)
	{
		lights = implementation->protectedLights ? *implementation->protectedLights : implementation->lights;
		objects = implementation->protectedObjects ? *implementation->protectedObjects : implementation->objects;
		if (implementation->environment)
			environment = implementation->environment->createReference();
		cameras = implementation->cameras;
	}
}

bool RRScene::save(const RRString& filename) const
{
	return s_sceneLoadersAndSavers.save(this,filename,"Scene");
}


/////////////////////////////////////////////////////////////////////////////
//
// RRMaterials loaders/savers

static LoadersAndSavers<RRMaterials> s_materialLoadersAndSavers;

void RRMaterials::registerLoader(const char* extensions, Loader* loader)
{
	s_materialLoadersAndSavers.registerLoader(extensions,loader);
}

void RRMaterials::registerSaver(const char* extensions, Saver* saver)
{
	s_materialLoadersAndSavers.registerSaver(extensions,saver);
}

const char* RRMaterials::getSupportedLoaderExtensions()
{
	return s_materialLoadersAndSavers.getSupportedLoaderExtensions();
}

const char* RRMaterials::getSupportedSaverExtensions()
{
	return s_materialLoadersAndSavers.getSupportedSaverExtensions();
}

bool RRMaterial::load(const RRString& _filename, RRFileLocator* _textureLocator)
{
	RRMaterials* loaded = s_materialLoadersAndSavers.load(_filename,_textureLocator,NULL,"Material");
	if (loaded && loaded->size())
	{
		copyFrom(*(*loaded)[0]);
		delete loaded;
		return true;
	}
	else
	{
		delete loaded;
		return false;
	}
}

bool RRMaterial::save(const RRString& filename) const
{
	RRMaterials materials;
	materials.push_back(const_cast<RRMaterial*>(this)); // we just need to pass const parameters through container of non-const materials
	return s_materialLoadersAndSavers.save(&materials,filename,"Material");
}

RRMaterials* RRMaterials::load(const RRString& _filename, RRFileLocator* _textureLocator)
{
	return s_materialLoadersAndSavers.load(_filename,_textureLocator,NULL,"Material");
}

bool RRMaterials::save(const RRString& filename) const
{
	return s_materialLoadersAndSavers.save(this,filename,"Material");
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
	RRReal scale = transformation.getScale().abs().avg();
	for (unsigned i=0;i<lights.size();i++)
	{
		RRLight* light = lights[i];
		if (light)
		{
			transformation.transformPosition(light->position);
			light->direction = transformation.getTransformedDirection(light->direction).normalized();
			light->polynom[1] /= scale;
			light->polynom[2] /= scale*scale;
			light->radius *= scale;
		}
	}
}

void RRScene::normalizeUnits(float currentUnitLengthInMeters)
{
	transform(RRMatrix3x4::scale(RRVec3(currentUnitLengthInMeters)));
}

void RRScene::normalizeUpAxis(unsigned currentUpAxis)
{
	switch (currentUpAxis)
	{
		case 0:
			transform(RRMatrix3x4(0.0f,-1,0,0, 1,0,0,0, 0,0,1,0));
			return;
		case 1:
			return;
		case 2:
			transform(RRMatrix3x4(1.0f,0,0,0, 0,0,1,0, 0,-1,0,0));
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

} // namespace rr
