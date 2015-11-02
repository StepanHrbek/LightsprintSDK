// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// 3d scene, collection of objects and lights.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRScene.h"
#ifdef _MSC_VER
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif
#include <unordered_set>
#include <regex> // wildcard matching
#include <boost/algorithm/string/replace.hpp> // wildcard matching
#include <boost/filesystem.hpp> // RRScene::save()
namespace bf = boost::filesystem;

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// wildcard matching

// for a long time, we had a simple extension test here
// but since RRFileLocator already depends on regex, we can afford proper wildcard matching at no extra cost

void escapeRegex(std::string &regex)
{
    boost::replace_all(regex, "\\", "\\\\");
    boost::replace_all(regex, "^", "\\^");
    boost::replace_all(regex, ".", "\\.");
    boost::replace_all(regex, "$", "\\$");
    boost::replace_all(regex, "|", "\\|");
    boost::replace_all(regex, "(", "\\(");
    boost::replace_all(regex, ")", "\\)");
    boost::replace_all(regex, "[", "\\[");
    boost::replace_all(regex, "]", "\\]");
    boost::replace_all(regex, "*", "\\*");
    boost::replace_all(regex, "+", "\\+");
    boost::replace_all(regex, "?", "\\?");
    boost::replace_all(regex, "/", "\\/");
}

bool matchTextWithWildcards(const std::string& text, std::string wildcardPattern)
{
    // Escape all regex special chars
    escapeRegex(wildcardPattern);

    // Convert chars '*?' back to their regex equivalents
    boost::replace_all(wildcardPattern, "\\?", ".");
    boost::replace_all(wildcardPattern, "\\*", ".*");

    std::regex pattern(wildcardPattern, std::regex::icase);

    return regex_match(text, pattern);
}


/////////////////////////////////////////////////////////////////////////////
//
// generic loaders/savers

#define SEPARATOR1 ';'
#define SEPARATOR2 ' ' // we might need different char later

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
		if (src[0]==SEPARATOR1 || src[0]==SEPARATOR2)
			src++;
		if (src[0]==':' && (src[1]==SEPARATOR1 || src[1]==SEPARATOR2 || src[1]==0)) // [#14] : is our symbol that matches everything, but unlike *.*, fileselector does not understand it
		{
			src += 1;
			return true; // : matches all, like *.*
		}
		while (*src!=0 && *src!=SEPARATOR1 && *src!=SEPARATOR2)
		{
			*dst = *src++;
			if (dst+1<extension+sizeof(extension))
				dst++;
			else
				RR_ASSERT(0); // our small buffer would overflow, extensionList contains long string not separated by ;
		}
		*dst = 0;
		if (*extension && matchTextWithWildcards(filename.c_str(),extension))
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
struct LoadersAndSavers1
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
			
			// convert all separators to SEPARATOR1
			for (char* c=loaderExtensions;*c;c++)
				if (*c==SEPARATOR2)
					*c = SEPARATOR1;
		}
		else
		{
			RRReporter::report(WARN,"Invalid argument (nullptr) in registerLoader().\n");
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
			
			// convert all separators to SEPARATOR1
			for (char* c=saverExtensions;*c;c++)
				if (*c==SEPARATOR2)
					*c = SEPARATOR1;
		}
		else
		{
			RRReporter::report(WARN,"Invalid argument (nullptr) in registerSaver().\n");
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
};

// LoadersAndSavers is split because buffer loading uses different set of parameters
// and we can use variadic templates only after dropping visual studio 2010, 2012
template <class RRClass>
struct LoadersAndSavers2 : public LoadersAndSavers1<RRClass>
{
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
			//RRReporter::report(WARN,"RRScene(nullptr), invalid argument.\n");
			return nullptr;
		}

		RRReportInterval report(INF1,"Loading %s %ls...\n",classname,_filename.w_str());

		// test whether loaders were registered
		if (loaders.empty())
		{
			RRReporter::report(WARN,"No loaders registered, call rr_io::registerLoaders() or RR%s::registerLoader() first.\n",classname);
			return nullptr;
		}

		// tell texture locator scene filename
		RRFileLocator* localTextureLocator = _textureLocator?_textureLocator:RRFileLocator::create();
		localTextureLocator->setParent(true,_filename);

		RRClass* loaded = nullptr;

		// test whether file exists (to properly report this common error)
		if (!localTextureLocator->exists(_filename))
		{
			RRReporter::report(WARN,"%ls does not exist.\n",_filename.w_str());
		}
		else
		{
			// attempt load
			bool loaderFound = false;
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
		}

		// cleanup
		localTextureLocator->setParent(false,_filename);
		if (!_textureLocator)
			delete localTextureLocator;

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
// RRBuffer loaders/savers

static LoadersAndSavers1<RRBuffer> s_bufferLoadersAndSavers;

void RRBuffer::registerLoader(const char* extensions, Loader* loader)
{
	s_bufferLoadersAndSavers.registerLoader(extensions, loader);
}

void RRBuffer::registerSaver(const char* extensions, Saver* saver)
{
	s_bufferLoadersAndSavers.registerSaver(extensions, saver);
}

const char* RRBuffer::getSupportedLoaderExtensions()
{
	return s_bufferLoadersAndSavers.getSupportedLoaderExtensions();
}

const char* RRBuffer::getSupportedSaverExtensions()
{
	return s_bufferLoadersAndSavers.getSupportedSaverExtensions();
}

RRBuffer* load_noncached(const RRString& _filename, const char* _cubeSideName[6])
{
	if (_filename.empty())
	{
		// don't warn, it probably happens
		return nullptr;
	}

	// test whether loaders were registered
	if (s_bufferLoadersAndSavers.loaders.empty())
	{
		RR_LIMITED_TIMES(1, RRReporter::report(WARN, "Can't load images, register loader first, see LightsprintIO.\n"));
		return nullptr;
	}

	RRBuffer* loaded = nullptr;

	// test whether file exists (to properly report this common error)
	bool sixfiles = wcsstr(_filename.w_str(),L"%s")!=nullptr;
	RRFileLocator fl;
	if (!sixfiles && !fl.exists(_filename))
	{
		// this probably never happens, we are called only for files that exist
		// when 1file does not exist, it is reported here
		// when 6file does not exist, it is not reported
		RRReporter::report(WARN,"%ls does not exist.\n",_filename.w_str());
	}
	else
	{
		// attempt load
		bool loaderFound = false;
		for (unsigned i=0;i<s_bufferLoadersAndSavers.loaders.size();i++)
		{
			if (extensionListMatches(_filename,s_bufferLoadersAndSavers.loaders[i].extensions.c_str()))
			{
				loaderFound = true;
				try
				{
					loaded = s_bufferLoadersAndSavers.loaders[i].loader(_filename,_cubeSideName);
				}
				catch (...)
				{
					RRReporter::report(WARN,"Buffer import ended by throwing C++ exception, something is broken.\n");
				}
				if (loaded)
				{
					break; // loaded, success
				}
				// load failed, but don't give up for cycle yet,
				//  it's possible that another loader for the same extension will succeed
			}
		}

		// report result
		if (!loaderFound)
		{
			RRReporter::report(WARN,"%ls not loaded, no loader for this extension was registered.\n",_filename.w_str());
		}
		else
		if (!loaded)
		{
			// when file does exist but loading fails, loaders usually don't report why
			RRReporter::report(WARN,"%ls load failed.\n",_filename.w_str());
		}
	}
	return loaded;
}

bool RRBuffer::save(const RRString& _filename, const char* _cubeSideName[6], const SaveParameters* _parameters)
{
	if (_filename.empty())
	{
		return false;
	}
	if (!this)
	{
		RRReporter::report(WARN, "Attempted nullptr->save().\n");
		return false;
	}
	if (s_bufferLoadersAndSavers.savers.empty())
	{
		RR_LIMITED_TIMES(1, RRReporter::report(WARN, "Can't save images, register saver first, see LightsprintIO.\n"));
		return false;
	}
	for (unsigned i = 0; i<s_bufferLoadersAndSavers.savers.size(); i++)
	{
		if (extensionListMatches(_filename,s_bufferLoadersAndSavers.savers[i].extensions.c_str()))
		{
			if (s_bufferLoadersAndSavers.savers[i].saver(this, _filename, _cubeSideName, _parameters))
			{
				filename = _filename; // [#36] filename of last successful save (although this could be weird if we save one frame of video)
				return true;
			}
		}
	}
	RRReporter::report(WARN, "Failed to save %ls.\n", _filename.w_str());
	return false;
}


/////////////////////////////////////////////////////////////////////////////
//
// preprocessed scene

class OneMaterialPerObject : public RRScene
{
public:
	OneMaterialPerObject(const RRScene& scene, unsigned copyLayerToMaterialLightmaps)
	{
		lights = scene.lights;
		cameras = scene.cameras;
		for (unsigned i=0;i<scene.objects.size();i++)
		{
			RRObjects origObjects;
			RRMaterials origMaterials;
			origObjects.push_back(scene.objects[i]);
			origObjects.getAllMaterials(origMaterials);
			RRObjects splitObjects = origObjects.mergeObjects(true);
			for (unsigned j=0;j<splitObjects.size();j++)
			{
				splitObjects[j]->name = scene.objects[i]->name;
				if (copyLayerToMaterialLightmaps!=UINT_MAX)
				{
					RRMaterial* m = new RRMaterial;
					m->copyFrom(*splitObjects[j]->faceGroups[0].material);
					m->lightmap.texture = scene.objects[i]->illumination.getLayer(copyLayerToMaterialLightmaps);
					splitObjects[j]->faceGroups[0].material = m;
					materials.push_back(m);
				}
				objects.push_back(splitObjects[j]);
			}
		}
	}
	~OneMaterialPerObject()
	{
		for (unsigned i=0;i<objects.size();i++)
			delete objects[i];
		for (unsigned i=0;i<materials.size();i++)
		{
			materials[i]->lightmap.texture = nullptr;
			delete materials[i];
		}
	}
private:
	RRMaterials materials;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRScene loaders/savers

static LoadersAndSavers2<RRScene> s_sceneLoadersAndSavers;

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
	protectedObjects = nullptr;
	protectedLights = nullptr;
	environment = nullptr;
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
	std::string ext = bf::path(RR_RR2PATH(filename)).extension().string();
	
	// these can be controlled by additional parameters sent to save(),
	// but let's guess what user wants and keep API simple, for now
	#define SUPPORTS_MULTIMATERIALS ext==".rr3"
	#define LIGHTMAP_LAYER 192837463 // default lightmap layer number in SceneViewer

	// directly save multimaterials to capable fileformats
	if (SUPPORTS_MULTIMATERIALS)
		return s_sceneLoadersAndSavers.save(this,filename,"Scene");

	// split multimaterials for other fileformats; and copy lightmaps to materials
	OneMaterialPerObject oneMaterialPerObject(*this,LIGHTMAP_LAYER);
	return s_sceneLoadersAndSavers.save(&oneMaterialPerObject,filename,"Scene");
}


/////////////////////////////////////////////////////////////////////////////
//
// RRMaterials loaders/savers

static LoadersAndSavers2<RRMaterials> s_materialLoadersAndSavers;

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
	RRMaterials* loaded = s_materialLoadersAndSavers.load(_filename,_textureLocator,nullptr,"Material");
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
	return s_materialLoadersAndSavers.load(_filename,_textureLocator,nullptr,"Material");
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
	implementation = nullptr;
	protectedObjects = nullptr;
	protectedLights = nullptr;
	environment = nullptr;
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

void RRScene::getAllBuffers(RRVector<RRBuffer*>& _buffers, const RRVector<unsigned>* _layers) const
{
	if (!this)
		return;
	typedef std::unordered_set<RRBuffer*> Set;
	Set set;
	// fill set
	// - original contents of vector
	for (unsigned i=0;i<_buffers.size();i++)
		set.insert(_buffers[i]);
	// - maps from lights
	for (unsigned i=0;i<lights.size();i++)
		set.insert(lights[i]->projectedTexture);
	// - maps from materials
	/* shorter but slower (more allocations)
	RRMaterials materials;
	objects.getAllMaterials(materials);
	for (int i=0;i<materials.size();i++)
		if (materials[i])
		{
			set.insert(materials[i]->diffuseReflectance.texture);
			set.insert(materials[i]->specularReflectance.texture);
			set.insert(materials[i]->diffuseEmittance.texture);
			set.insert(materials[i]->specularTransmittance.texture);
			set.insert(materials[i]->bumpMap.texture);
			// skipping lightmap in material, we use illumination layers instead
		}
	*/
	for (int i=0;i<(int)objects.size();i++)
	{
		const RRObject* object = objects[i];
		if (object)
		{
			const RRObject::FaceGroups& faceGroups = object->faceGroups;
			for (unsigned g=0;g<faceGroups.size();g++)
			{
				RRMaterial* m = faceGroups[g].material;
				if (m)
				{
					set.insert(m->diffuseReflectance.texture);
					set.insert(m->specularReflectance.texture);
					set.insert(m->diffuseEmittance.texture);
					set.insert(m->specularTransmittance.texture);
					set.insert(m->bumpMap.texture);
					// skipping lightmap in material, we use illumination layers instead
				}
			}
		}
	}
	// - environment
	set.insert(environment);
	// - illumination layers
	if (_layers)
	{
		for (unsigned i=0;i<objects.size();i++)
			for (unsigned j=0;j<_layers->size();j++)
				set.insert(objects[i]->illumination.getLayer((*_layers)[j]));
	}
	// copy set back to vector
	_buffers.clear();
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			_buffers.push_back(*i);
}

RRScene::~RRScene()
{
	delete environment;
	delete protectedLights;
	delete protectedObjects;
	delete implementation;
}

} // namespace rr
