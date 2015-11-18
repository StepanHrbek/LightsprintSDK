// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Non-intrusive serialization of Lightsprint structures.
// --------------------------------------------------------------------------

#ifndef RRSERIALIZATION_H
#define RRSERIALIZATION_H

// This is optional header-only extension of LightsprintCore library.
// You can use it to serialize Lightsprint structures, but only if you have boost, http://boost.org.
// If you don't have boost, simply don't include this header, e.g. by disabling #define SUPPORT_LIGHTSPRINT.

#include "Lightsprint/RRScene.h"
#include <boost/serialization/binary_object.hpp> // either install boost or don't serialize
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include <unordered_set>
#include <boost/filesystem.hpp> // is_complete
#include <boost/locale.hpp> // boost::locale::normalize()

namespace bf = boost::filesystem;

//------------------------- runtime declaration --------------------------------

RR_API extern class SerializationRuntime* g_serializationRuntime;

namespace boost {
namespace serialization {

	class RRBufferProxy;
	class RRMeshProxy;

}
}

// Global serialization state, single instance must exist during serialization.
//
// Q: How do I use it?
// A: Add "SerializationRuntime sr(NULL);" at the beginning of your serialization code.
//
// Q: Is it thread safe?
// A: No, serialization state is stored in single global variable, multiple threads serializing at the same time would break it.

// Q: Global variable is ugly, is it necessary?
// A: No. Proper solution is to make serialization state part of Archive class.
//    It would make it legal to serialize in multiple threads at the same time.
//    However, all attempts to do so generically (without reimplementing archive) have failed.
//
// Q: If it has to be global, why not simple static variable declared in this header? Why is it extern, with pointer living in core?
// A: If we omit extern and let it live here,
//    OSX (gcc 4.2) compiled libraries that include RRSerialization.h write to different variables,
//    while serialization reads from one variable, so there is a mess, some textures are not located etc.
//    No such problem exists with Visual C++.
//
// Q: If it has to be global, can we at least make it thread safe by storing pointer to state in TLS?
// A: Maybe. So far we serialize serially, so it's low priority.
//
// Q: What happens when multiple SerializationRuntime exist?
// A: The newest one is visible. When it destructs, older one becomes visible again.

class SerializationRuntime
{
public:
	// Helps locate textures.
	rr::RRFileLocator* textureLocator;

	// Helps load 3:4 cross maps as cubemaps.
	bool nextBufferIsCube;

	// For diagnostics only.
	const char* origin;

	// Helps save each instance only once.
	std::unordered_set<boost::serialization::RRBufferProxy*> bufferProxyInstances;
	std::unordered_set<boost::serialization::RRMeshProxy*> meshProxyInstances;

	inline SerializationRuntime(rr::RRFileLocator* fileLocator, const char* origin);
	inline ~SerializationRuntime();

	static inline bool exists();

private:
	SerializationRuntime* backup;
};


//------------------------- filename portability --------------------------------

// to be called on deserialized paths
// fixes Windows paths on non Windows platforms
// other solution would be to save only generic paths, but we already have many datafiles saved with Windows native paths
static void fixPath(rr::RRString& filename)
{
	try
	{
		boost::locale::generator gen;
		std::locale loc = gen(""); 

		// convert to wstring
		std::wstring wstr = RR_RR2STDW(filename);

	#ifdef _WIN32
		// convert unicode to form used by Windows paths
		// Windows composes OSX filenames with NFC (OSX is said to use NFKC, but boost NFKC with "winapi" backend keeps filenames decomposed, is it error?)
		std::wstring wstr2 = boost::locale::normalize(wstr,boost::locale::norm_nfc,loc);
	#else
		// convert unicode to form used by OSX paths
		// OSX decomposes Windows filenames with NFD
		std::wstring wstr2 = boost::locale::normalize(wstr,boost::locale::norm_nfd,loc);
		// Windows tends to accept both / and \, but other OSes expect /, let's convert \ to / on read
		std::replace(wstr2.begin(),wstr2.end(),'\\','/');
	#endif

		// convert from wstring
		filename = RR_STDW2RR(wstr2); // [#36] no changes to path, just encoding
	}
	catch(...)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Unicode normalization failed, data compatibility between platforms not guaranteed.\n"));
	}
}

//--------------------------- serialization ----------------------------------

namespace boost {
namespace serialization {

// switches between readable array and fast binary object
//#define make_array_or_binary(a,s) make_array(a,s)
#define make_array_or_binary(a,s) make_binary_object(a,(s)*sizeof(*(a)))

//------------------------------ RRVec2 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec2& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
}

//------------------------------ RRVec3 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec3& a, const unsigned int version)
{
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("z",a.z);
}

//------------------------------ RRVec4 -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRVec4& a, const unsigned int version)
{
	// a) inherited variables are stored inside <base>
	//ar & make_nvp("base",boost::serialization::base_object<rr::RRVec3>(a));
	// b) it is unclear what version is used
	//rr::RRVec3& aa = a;
	//serialize(ar,aa,version);
	// c) no inheritance
	ar & make_nvp("x",a.x);
	ar & make_nvp("y",a.y);
	ar & make_nvp("z",a.z);
	ar & make_nvp("w",a.w);
}

//------------------------------ RRMatrix3x4 -------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRMatrix3x4& a, const unsigned int version)
{
	save(ar,a.m);
}

template<class Archive>
void load(Archive & ar, rr::RRMatrix3x4& a, const unsigned int version)
{
	load(ar,a.m);
}

//------------------------------ RRString ------------------------------------

// boost can't portably serialize wstring, here we convert it to portable utf8 string

static void utf32or16to8(const wchar_t* utf32, std::string& utf8)
{
	bool invalid = false;
	for (unsigned i=0;utf32[i];i++)
	{
		unsigned code = utf32[i]; // read from utf32
		if (sizeof(wchar_t)<4 && code>=0xd800 && code<0xe000) {if (utf32[i+1]) { // read from utf16
			if (code<0xdc00 && utf32[i+1]>=0xdc00 && utf32[i+1]<0xe000) {code = 0x10000+((code-0xd800)<<10)+(utf32[i+1]-0xdc00); i++;} else
			if (code>=0xdc00 && utf32[i+1]>=0xd800 && utf32[i+1]<0xdc00) {code = 0x10000+((utf32[i+1]-0xd800)<<10)+(code-0xdc00); i++;} else
			invalid = true;} else invalid = true;}
		if (code<0x80) {utf8 += code;}
		else if (code<0x800) {utf8 += 192+(code>>6); utf8 += 128+(code&63);}
		else if (code<0x10000) {utf8 += 224+(code>>12); utf8 += 128+((code>>6)&63); utf8 += 128+(code&63);}
		else if (code<0x110000) {utf8 += 240+(code>>18); utf8 += 128+((code>>12)&63); utf8 += 128+((code>>6)&63); utf8 += 128+(code&63);}
		else invalid = true;
	}
	if (invalid)
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Serializing invalid wide string: %ls\n",utf32));
}

static void utf8to16or32(const char* utf8, std::wstring& utf32)
{
	bool invalid = false;
	for (unsigned i=0;utf8[i];i++)
	{
		unsigned char c = utf8[i];
		if (c<0x80) {utf32 += c;}
		else if (c<0xc2) {invalid = true;}
		else if (c<0xe0) {if ((utf8[i+1]&192)==128) {utf32 += ((c&31)<<6)+(utf8[i+1]&63); i += 1;} else invalid = true;}
		else if (c<0xf0) {if ((utf8[i+1]&192)==128 && (utf8[i+2]&192)==128) {unsigned code = ((c&15)<<12)+((utf8[i+1]&63)<<6)+(utf8[i+2]&63); i += 2;
			if (sizeof(wchar_t)>=4) utf32 += code; // write to utf32
			else {if (code<0xd800 || code>0xe000) utf32 += code; else invalid = true;} // write to utf16
			} else invalid = true;}
		else if (c<0xf5) {if ((utf8[i+1]&192)==128 && (utf8[i+2]&192)==128 && (utf8[i+3]&192)==128) {unsigned code = ((c&7)<<18)+((utf8[i+1]&63)<<12)+((utf8[i+2]&63)<<6)+(utf8[i+3]&63); i += 3;
			if (sizeof(wchar_t)>=4) utf32 += code; // write to utf32
			else {utf32 += 0xd800+((code-0x10000)>>10); utf32 += 0xdc00+((code-0x10000)&0x3ff);} // write to utf16
			} else invalid = true;}
		else invalid = true;
	}
	if (invalid)
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Deserializing invalid utf8 string: %s\n",utf8));
}

template<class Archive>
void save(Archive & ar, const rr::RRString& a, const unsigned int version)
{
	std::string s8;
	utf32or16to8(a.w_str(),s8);
	save(ar,s8);
}

template<class Archive>
void load(Archive & ar, rr::RRString& a, const unsigned int version)
{
	std::string s8;
	load(ar,s8);
	if (version<1)
	{
		a = RR_STD2RR(s8);
	}
	else
	{
		std::wstring s32;
		utf8to16or32(s8.c_str(),s32);
		a = RR_STDW2RR(s32);
	}
}

/*/------------------------------ wxString -------------------------------------

#define SERIALIZE_WXSTRING(name,wxstring,utf8) \
	{ \
		std::string s; \
		if (Archive::is_saving::value) s = wxstring.ToUTF8(); \
		ar & make_nvp(name,s); \
		if (Archive::is_loading::value) wxstring = utf8 ? wxString::FromUTF8(s.c_str()) : s; \
	}
*/

//------------------------------ RRBuffer -------------------------------------

// saves buffer contents, shared by .rr3 and .rrbuffer
template<class Archive>
void saveBufferContents(Archive & ar, rr::RRBuffer& a, const unsigned int version)
{
	rr::RRBufferType type = a.getType();
	ar & make_nvp("type",type);
	unsigned width = a.getWidth();
	ar & make_nvp("width",width);
	unsigned height = a.getHeight();
	ar & make_nvp("height",height);
	unsigned depth = a.getDepth();
	ar & make_nvp("depth",depth);
	rr::RRBufferFormat format = a.getFormat();
	ar & make_nvp("format",format);
	bool scaled = a.getScaled();
	ar & make_nvp("scaled",scaled);
	unsigned char* data = a.lock(rr::BL_READ); // if you call saves in parallel (why?), they may lock in parallel. locking is thread safe in our buffer implementations, but is allowed to be unsafe in other people's implementations.
	binary_object bo(data,a.getBufferBytes());
	ar & make_nvp("data",bo);
	a.unlock();
}

// loads buffer contents, shared by .rr3 and .rrbuffer
template<class Archive>
rr::RRBuffer* loadBufferContents(Archive & ar, const unsigned int version)
{
	rr::RRBufferType type;
	ar & make_nvp("type",type);
	unsigned width,height,depth;
	ar & make_nvp("width",width);
	ar & make_nvp("height",height);
	ar & make_nvp("depth",depth);
	rr::RRBufferFormat format;
	ar & make_nvp("format",format);
	bool scaled;
	ar & make_nvp("scaled",scaled);
	rr::RRBuffer* buffer = rr::RRBuffer::create(type,width,height,depth,format,scaled,NULL);
	binary_object bo(buffer->lock(rr::BL_DISCARD_AND_WRITE),buffer->getBufferBytes());
	ar & make_nvp("data",bo);
	buffer->unlock();
	return buffer;
}

// Instead of RRBuffer, we serialize dynamically created proxy object.
// It's necessary to circumvent boost limitations:
//  - no custom allocation (http://lists.boost.org/boost-users/2005/05/11773.php)
//  - all derived classes must be registered
// If two pointers point to the same buffer, only one proxy is created.
// We store set of proxy instances in g_serializationRuntime, they must be alive during load, we free them afterwards.

class RRBufferProxy
{
public:
	rr::RRBuffer* buffer;

	RRBufferProxy()
	{
		RR_ASSERT(g_serializationRuntime);
		buffer = NULL;
		g_serializationRuntime->bufferProxyInstances.insert(this);
	}
	~RRBufferProxy()
	{
		RR_ASSERT(g_serializationRuntime);
		delete buffer; // all unique buffers are created and deleted once
		g_serializationRuntime->bufferProxyInstances.erase(this);
	}
};

#define prefix_buffer(b)          (*(RRBufferProxy**)&(b))
#define postfix_buffer(Archive,b) {if (Archive::is_loading::value && b) b = prefix_buffer(b)->buffer?prefix_buffer(b)->buffer->createReference():NULL;} // all non-unique buffers get their own reference


template<class Archive>
void save(Archive & ar, const RRBufferProxy& aa, const unsigned int version)
{
	// only unique non-NULL buffers get here
	rr::RRBuffer& a = *(rr::RRBuffer*)&aa;
	if (a.filename.empty())
	{
		ar & make_nvp("filename",a.filename);
		saveBufferContents(ar,a,version);
	}
	else
	{
		// saved paths must be absolute, necessary for proper relocation at load time
		// saved type must be RRString (saving std::string and loading RRString works if scene has at least 1 light or material, fails in empty scene)
		rr::RRString absolute = RR_PATH2RR(bf::system_complete(RR_RR2PATH(a.filename)));
		ar & make_nvp("filename",(a.isStub() || a.filename=="c@pture")?a.filename:absolute); // otherwise "c@pture" would be saved as "c:/foo/c@pture"
	}
}

template<class Archive>
void load(Archive & ar, RRBufferProxy& a, const unsigned int version)
{
	// only unique non-NULL buffers get here
	rr::RRString filename;
	ar & make_nvp("filename",filename);
	if (filename.empty())
	{
		a.buffer = loadBufferContents(ar,version);
	}
	else
	{
		fixPath(filename);
		RR_ASSERT(g_serializationRuntime);
		if (g_serializationRuntime->nextBufferIsCube)
			a.buffer = rr::RRBuffer::loadCube(filename,g_serializationRuntime->textureLocator);
		else
			a.buffer = rr::RRBuffer::load(filename,NULL,g_serializationRuntime->textureLocator);
	}
}

//----------------------------- RRVector<C> ------------------------------------

template<class Archive, class C>
void save(Archive & ar, const rr::RRVector<C>& a, const unsigned int version)
{
	unsigned count = a.size();
	ar & make_nvp("count",count);
	for (unsigned i=0;i<count;i++)
	{
		ar & make_nvp("item",a[i]);
	}
}

template<class Archive, class C>
void load(Archive & ar, rr::RRVector<C>& a, const unsigned int version)
{
	RR_ASSERT(!a.size());
	a.clear();
	unsigned count;
	ar & make_nvp("count",count);
	while (count--)
	{
		C item;
		ar & make_nvp("item", item);
		a.push_back(item);
	}
}

template<class Archive, class C>
void save(Archive & ar, const rr::RRVector<C*>& a, const unsigned int version)
{
	unsigned count = 0;
	for (unsigned i=0;i<a.size();i++)
		if (a[i])
			count++;
	ar & make_nvp("count",count);
	for (unsigned i=0;i<a.size();i++)
		if (a[i]) // skip NULL items
			ar & make_nvp("item",*a[i]);
}

template<class Archive, class C>
void load(Archive & ar, rr::RRVector<C*>& a, const unsigned int version)
{
	RR_ASSERT(!a.size());
	a.clear();
	unsigned count;
	ar & make_nvp("count",count);
	while (count--)
	{
		C* item = new C;
		ar & make_nvp("item", *item);
		a.push_back(item);
	}
}

template<class Archive, class C>
void serialize(Archive & ar, rr::RRVector<C>& a, const unsigned int file_version)
{
	boost::serialization::split_free(ar, a, file_version);
}

//------------------------------ RRSideBits -------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRSideBits& a, const unsigned int version)
{
	bool renderFrom = a.renderFrom;
	ar & make_nvp("renderFrom",renderFrom);
	bool emitTo = a.emitTo;
	ar & make_nvp("emitTo",emitTo);
	bool catchFrom = a.catchFrom;
	ar & make_nvp("catchFrom",catchFrom);
	bool legal = a.legal;
	ar & make_nvp("legal",legal);
	bool receiveFrom = a.receiveFrom;
	ar & make_nvp("receiveFrom",receiveFrom);
	bool reflect = a.reflect;
	ar & make_nvp("reflect",reflect);
	bool transmitFrom = a.transmitFrom;
	ar & make_nvp("transmitFrom",transmitFrom);
}

template<class Archive>
void load(Archive & ar, rr::RRSideBits& a, const unsigned int version)
{
	bool renderFrom;
	ar & make_nvp("renderFrom",renderFrom);
	a.renderFrom = renderFrom;
	bool emitTo;
	ar & make_nvp("emitTo",emitTo);
	a.emitTo = emitTo;
	bool catchFrom;
	ar & make_nvp("catchFrom",catchFrom);
	a.catchFrom = catchFrom;
	bool legal;
	ar & make_nvp("legal",legal);
	a.legal = legal;
	bool receiveFrom;
	ar & make_nvp("receiveFrom",receiveFrom);
	a.receiveFrom = receiveFrom;
	bool reflect;
	ar & make_nvp("reflect",reflect);
	a.reflect = reflect;
	bool transmitFrom;
	ar & make_nvp("transmitFrom",transmitFrom);
	a.transmitFrom = transmitFrom;
}

//------------------------------ RRMaterial::Property -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRMaterial::Property& a, const unsigned int version)
{
	ar & make_nvp("color",a.color);
	ar & make_nvp("texture",prefix_buffer(a.texture)); postfix_buffer(Archive,a.texture);
	ar & make_nvp("texcoord",a.texcoord);
}

//------------------------------ RRMaterial -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRMaterial& a, const unsigned int version)
{
	if (Archive::is_loading::value)
	{
		// if we don't call reset(), reading old rr3 would not initialize new attributes
		// for example bumpMapTypeHeight would be random, boost serialization asserts when it is not 0 or 1
		a.reset(false);
	}
	ar & make_nvp("name",a.name);
	ar & make_nvp("sideBits",a.sideBits);
	ar & make_nvp("diffuseReflectance",a.diffuseReflectance);
	ar & make_nvp("diffuseEmittance",a.diffuseEmittance);
	ar & make_nvp("specularReflectance",a.specularReflectance);
	if (version>0)
	{
		ar & make_nvp("specularModel",a.specularModel);
		ar & make_nvp("specularShininess",a.specularShininess);
	}
	ar & make_nvp("specularTransmittance",a.specularTransmittance);
	ar & make_nvp("specularTransmittanceInAlpha",a.specularTransmittanceInAlpha);
	ar & make_nvp("specularTransmittanceKeyed",a.specularTransmittanceKeyed);
	if (version>4)
	{
		ar & make_nvp("specularTransmittanceThreshold",a.specularTransmittanceThreshold);
	}
	if (version>3)
	{
		ar & make_nvp("specularTransmittanceMapInverted",a.specularTransmittanceMapInverted);
	}
	ar & make_nvp("refractionIndex",a.refractionIndex);
	if (version>1)
	{
		ar & make_nvp("normalMap",a.bumpMap);
	}
	ar & make_nvp("lightmapTexcoord",a.lightmapTexcoord);
	ar & make_nvp("minimalQualityForPointMaterials",a.minimalQualityForPointMaterials);

	if (version<3 && Archive::is_loading::value)
	{
		// older versions had default 0, now we want default 1
		a.bumpMap.color = rr::RRVec3(1);
	}
	if (version>2)
	{
		ar & make_nvp("bumpType",a.bumpMapTypeHeight);
	}

	// sometimes texture changes when material sits serialized on disk
	// should we update colors from textures after deserialization?
	// a) YES don't trust serialized colors, update color always on load, update keying if color did change
	//    - slow
	//    + sometimes more accurate (when textures did change)
	// b) update color+keying on request
	//    - not automatic, users prefer if everything works automatically
	// c) update color+keying when texture changes
	//    . serialize hash (jen vedle filename, neni nutny pri embedu), compare on load: bool needsUpdate=loaded_hash!=calculated_hash;
	//    - slow
	// d) update color+keying only when texture starts/stops being stub
	//    - does not update after texture change
	//    - needs isStub to be saved with filename
	if (Archive::is_loading::value)
	{
		// backup average colors
		// if they differ after updateColorsFromTextures, we know that texture has changed
		rr::RRVec3 specularTransmittanceColor = a.specularTransmittance.color;
		rr::RRVec3 bumpMapColor = a.bumpMap.color;

		// get average colors from textures
		{
			rr::RRColorSpace* colorSpace = rr::RRColorSpace::create_sRGB();
			a.updateColorsFromTextures(colorSpace,rr::RRMaterial::UTA_KEEP,false); // but don't update from stubs, we have valid colors (unlike all other scene formats)
			delete colorSpace;
		}

		// autodetect InAlpha
		// isolated load without textures never sets InAlpha, we have to do it when we detect that texture has changed
		// must be done before detecting keying
		if (a.specularTransmittance.texture && specularTransmittanceColor!=a.specularTransmittance.color)
			a.specularTransmittanceInAlpha = a.specularTransmittance.texture->getFormat()==rr::BF_RGBA || a.specularTransmittance.texture->getFormat()==rr::BF_RGBAF;

		// autodetect keying
		// a) always autodetect
		//    - user can't manually change keying, it always returns back after load
		// b) do nothing, keep flag from last time
		//    - when texture changes on disk, user has to manually fix keying
		//    - when isolated load loads scene without textures and saves rr3 
		// c) YES autodetect only if average color did change
		if (specularTransmittanceColor!=a.specularTransmittance.color)
			a.updateKeyingFromTransmittance();

		// optimize material flags
		a.updateSideBitsFromColors();

		// autodetect bump map type
		if (bumpMapColor!=a.bumpMap.color)
			a.updateBumpMapType();
	}
}

//----------------------------- RRMaterials ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRMaterials& a, const unsigned int version)
{
	rr::RRVector<rr::RRMaterial*>& aa = a;
	serialize(ar,aa,version);
}

//------------------------------ RRLight ------------------------------------

#ifndef DONT_SERIALIZE_RRLIGHT
template<class Archive>
void serialize(Archive & ar, rr::RRLight& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	if (version>1)
	{
		ar & make_nvp("enabled", a.enabled);
	}
	ar & make_nvp("type",a.type);
	ar & make_nvp("position",a.position);
	ar & make_nvp("direction",a.direction);
	ar & make_nvp("outerAngleRad",a.outerAngleRad);
	ar & make_nvp("radius",a.radius);
	ar & make_nvp("color",a.color);
	ar & make_nvp("distanceAttenuationType",a.distanceAttenuationType);
	ar & make_nvp("polynom",a.polynom);
	ar & make_nvp("fallOffExponent",a.fallOffExponent);
	ar & make_nvp("spotExponent",a.spotExponent);
	ar & make_nvp("fallOffAngleRad",a.fallOffAngleRad);
	ar & make_nvp("castShadows",a.castShadows);
	ar & make_nvp("rtProjectedTexture",prefix_buffer(a.rtProjectedTexture)); postfix_buffer(Archive,a.rtProjectedTexture);
	if (version<1)
	{
		float rtMaxShadowSize;
		ar & make_nvp("rtMaxShadowSize",rtMaxShadowSize);
	}
	if (version>0)
	{
		ar & make_nvp("rtNumShadowmaps",a.rtNumShadowmaps);
	}
	if (version>2)
	{
		ar & make_nvp("rtShadowmapSize",a.rtShadowmapSize);
	}
	if (version>4)
	{
		ar & make_nvp("rtShadowmapBias",a.rtShadowmapBias);
	}
	if (version>3)
	{
		ar & make_nvp("directLambertScaled",a.directLambertScaled);
	}
	// skip customData;
}

//----------------------------- RRLights ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRLights& a, const unsigned int version)
{
	rr::RRVector<rr::RRLight*>& aa = a;
	serialize(ar,aa,version);
}
#endif // !DONT_SERIALIZE_RRLIGHT

//------------------------------ RRMesh::Triangle -------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRMesh::Triangle& a, const unsigned int version)
{
	save(ar,a.m);
}

template<class Archive>
void load(Archive & ar, rr::RRMesh::Triangle& a, const unsigned int version)
{
	load(ar,a.m);
}

//------------------------------ RRMeshArrays ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRMeshArrays& a, const unsigned int version)
{
	// sizes
	ar & make_nvp("numTriangles",a.numTriangles);
	ar & make_nvp("numVertices",a.numVertices);
	rr::RRVector<unsigned> texcoords;
	for (unsigned i=0;i<a.texcoord.size();i++)
		if (a.texcoord[i])
			texcoords.push_back(i);
	ar & make_nvp("texcoords",texcoords);
	bool tangents = a.tangent && a.bitangent;
	ar & make_nvp("tangents",tangents);

	// we save/load mesh arrays in binary form (faster), platform differences must be compensated here
	RR_STATIC_ASSERT(sizeof(a.triangle[0])==12,"TODO: change unsigned in Triangle to uint32_t.");
	RR_STATIC_ASSERT(sizeof(a.position[0])==12,"What, RRReal is not 32bit?");
	#ifdef RR_BIG_ENDIAN
	#warning TODO: Toggle endianity here to ensure .rr3 compatibility on all platforms.
	#endif

	// arrays
	ar & make_nvp("triangle",make_array_or_binary(a.triangle,a.numTriangles));
	ar & make_nvp("position",make_array_or_binary(a.position,a.numVertices));
	ar & make_nvp("normal",make_array_or_binary(a.normal,a.numVertices));
	if (tangents)
	{
		ar & make_nvp("tangent",make_array_or_binary(a.tangent,a.numVertices));
		ar & make_nvp("bitangent",make_array_or_binary(a.bitangent,a.numVertices));
	}
	for (unsigned i=0;i<a.texcoord.size();i++)
		if (a.texcoord[i])
			ar & make_nvp("texcoord",make_array_or_binary(a.texcoord[i],a.numVertices));

	// unwrap
	ar & make_nvp("unwrapChannel",a.unwrapChannel);
	ar & make_nvp("unwrapWidth",a.unwrapWidth);
	ar & make_nvp("unwrapHeight",a.unwrapHeight);
}

template<class Archive>
void load(Archive & ar, rr::RRMeshArrays& a, const unsigned int version)
{
	// sizes
	unsigned numTriangles;
	ar & make_nvp("numTriangles",numTriangles);
	unsigned numVertices;
	ar & make_nvp("numVertices",numVertices);
	rr::RRVector<unsigned> texcoords;
	ar & make_nvp("texcoords",texcoords);
	bool tangents;
	ar & make_nvp("tangents",tangents);
	a.resizeMesh(numTriangles,numVertices,&texcoords,tangents,false);

	// we save/load mesh arrays in binary form (faster), platform differences must be compensated here
	RR_STATIC_ASSERT(sizeof(a.triangle[0])==12,"TODO: change unsigned in Triangle to uint32_t.");
	RR_STATIC_ASSERT(sizeof(a.position[0])==12,"What, RRReal is not 32bit?");
	#ifdef RR_BIG_ENDIAN
	#warning TODO: Toggle endianity here to ensure .rr3 compatibility on all platforms.
	#endif

	// arrays
	ar & make_nvp("triangle",make_array_or_binary(a.triangle,numTriangles));
	ar & make_nvp("position",make_array_or_binary(a.position,numVertices));
	ar & make_nvp("normal",make_array_or_binary(a.normal,numVertices));
	if (tangents)
	{
		ar & make_nvp("tangent",make_array_or_binary(a.tangent,numVertices));
		ar & make_nvp("bitangent",make_array_or_binary(a.bitangent,numVertices));
	}
	for (unsigned i=0;i<a.texcoord.size();i++)
		if (a.texcoord[i])
			ar & make_nvp("texcoord",make_array_or_binary(a.texcoord[i],numVertices));

	if (version>0)
	{
		// unwrap
		ar & make_nvp("unwrapChannel",a.unwrapChannel);
		ar & make_nvp("unwrapWidth",a.unwrapWidth);
		ar & make_nvp("unwrapHeight",a.unwrapHeight);
	}
}

//------------------------------ RRMeshProxy -------------------------------------

// Instead of RRMesh, we serialize dynamically created proxy object.
// It's necessary to circumvent boost limitations:
//  - no custom allocation (http://lists.boost.org/boost-users/2005/05/11773.php)
//  - all derived classes must be registered
// We store set of proxy instances in g_serializationRuntime, they must be alive during load, we free them afterwards.

class RRMeshProxy
{
public:
	rr::RRMeshArrays* mesh;

	RRMeshProxy()
	{
		RR_ASSERT(g_serializationRuntime);
		g_serializationRuntime->meshProxyInstances.insert(this);
	}
	~RRMeshProxy()
	{
		RR_ASSERT(g_serializationRuntime);
		g_serializationRuntime->meshProxyInstances.erase(this);
	}
};

template<class Archive>
void save(Archive & ar, const RRMeshProxy& a, const unsigned int version)
{
	// here we get only unique non-NULL meshes
	// we can't save any mesh, only RRMeshArrays, so conversion might be necessary
	rr::RRMesh* mesh = (rr::RRMesh*)&a;
	rr::RRMeshArrays* meshArrays = dynamic_cast<rr::RRMeshArrays*>(mesh);
	if (!meshArrays)
	{
		rr::RRVector<unsigned> texcoords;
		mesh->getUvChannels(texcoords);
		meshArrays = mesh->createArrays(true,texcoords,true);
	}
	save(ar,*meshArrays,version);
	if (meshArrays!=mesh)
		delete meshArrays;
}

template<class Archive>
void load(Archive & ar, RRMeshProxy& a, const unsigned int version)
{
	// here we get only unique non-NULL meshes
	a.mesh = new rr::RRMeshArrays;
	load(ar,*a.mesh,version);
}

//------------------------------ RRObject::FaceGroup -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRObject::FaceGroup& a, const unsigned int version)
{
	ar & make_nvp("numTriangles",a.numTriangles);
	ar & make_nvp("material",a.material);
}

//----------------------------- RRObject::FaceGroups ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRObject::FaceGroups& a, const unsigned int version)
{
	rr::RRVector<rr::RRObject::FaceGroup>& aa = a;
	serialize(ar,aa,version);
}

//------------------------------ RRObject ------------------------------------

template<class Archive>
void save(Archive & ar, const rr::RRObject& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	ar & make_nvp("faceGroups",a.faceGroups);
	rr::RRMatrix3x4 matrix(a.getWorldMatrixRef());
	ar & make_nvp("worldMatrix",matrix);
	ar & make_nvp("isDynamic",a.isDynamic);
	ar & make_nvp("enabled",a.enabled);
	{
		RRMeshProxy* proxy = (RRMeshProxy*)a.getCollider()->getMesh();
		if (!proxy)
			rr::RRReporter::report(rr::ERRO,"Saved NULL mesh.\n");
		ar & make_nvp("mesh",proxy);
	}
}

template<class Archive>
void load(Archive & ar, rr::RRObject& a, const unsigned int version)
{
	ar & make_nvp("name",a.name);
	ar & make_nvp("faceGroups",a.faceGroups);
	{
		rr::RRMatrix3x4 worldMatrix;
		ar & make_nvp("worldMatrix",worldMatrix);
		a.setWorldMatrix(&worldMatrix);
	}
	if (version>0)
	{
		ar & make_nvp("isDynamic",a.isDynamic);
	}
	if (version>1)
	{
		ar & make_nvp("enabled",a.enabled);
	}
	{
		RRMeshProxy* proxy = NULL;
		ar & make_nvp("mesh",proxy);
		if (proxy && proxy->mesh)
		{
			bool aborting = false;
			a.setCollider(rr::RRCollider::create(proxy->mesh,NULL,rr::RRCollider::IT_LINEAR,aborting));
		}
		else
		{
			rr::RRReporter::report(rr::ERRO,"Loaded NULL mesh.\n");
		}
	}
}

//----------------------------- RRObjects ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRObjects& a, const unsigned int version)
{
	rr::RRVector<rr::RRObject*>& aa = a;
	serialize(ar,aa,version);
}

//----------------------------- RRCamera ------------------------------------

#ifndef DONT_SERIALIZE_RRCAMERA
template<class Archive>
void save(Archive & ar, const rr::RRCamera& a, const unsigned int version)
{
	rr::RRVec3 position = a.getPosition();
	rr::RRVec3 yawPitchRollRad = a.getYawPitchRollRad();
	bool orthogonal = a.isOrthogonal();
	float aspect = a.getAspect();
	float fieldOfViewVerticalDeg = a.getFieldOfViewVerticalDeg();
	float anear = a.getNear();
	float afar = a.getFar();
	float orthoSize = a.getOrthoSize();
	rr::RRVec2 screenCenter = a.getScreenCenter();

	ar & make_nvp("position",position);
	ar & make_nvp("yawPitchRollRad",yawPitchRollRad);
	ar & make_nvp("orthogonal",orthogonal);
	ar & make_nvp("aspect",aspect);
	ar & make_nvp("fieldOfViewVerticalDeg",fieldOfViewVerticalDeg);
	ar & make_nvp("near",anear);
	ar & make_nvp("far",afar);
	ar & make_nvp("orthoSize",orthoSize);
	ar & make_nvp("screenCenter",screenCenter);
	ar & make_nvp("eyeSeparation",a.eyeSeparation);
	ar & make_nvp("focalLength",a.displayDistance);
	ar & make_nvp("apertureDiameter",a.apertureDiameter);
	ar & make_nvp("dofNear",a.dofNear);
	ar & make_nvp("dofFar",a.dofFar);
}

template<class Archive>
void load(Archive & ar, rr::RRCamera& a, const unsigned int version)
{
	rr::RRVec3 position;
	rr::RRVec3 yawPitchRollRad;
	bool orthogonal;
	float aspect;
	float fieldOfViewVerticalDeg;
	float anear;
	float afar;
	float orthoSize;
	rr::RRVec2 screenCenter;

	ar & make_nvp("position",position);
	ar & make_nvp("yawPitchRollRad",yawPitchRollRad);
	ar & make_nvp("orthogonal",orthogonal);
	ar & make_nvp("aspect",aspect);
	ar & make_nvp("fieldOfViewVerticalDeg",fieldOfViewVerticalDeg);
	ar & make_nvp("near",anear);
	ar & make_nvp("far",afar);
	ar & make_nvp("orthoSize",orthoSize);
	ar & make_nvp("screenCenter",screenCenter);

	a.setView(position,yawPitchRollRad);
	a.setProjection(orthogonal,aspect,fieldOfViewVerticalDeg,anear,afar,orthoSize,screenCenter);

	if (version>0)
	{
		ar & make_nvp("eyeSeparation",a.eyeSeparation);
		ar & make_nvp("focalLength",a.displayDistance);
	}
	if (version>2)
	{
		ar & make_nvp("apertureDiameter",a.apertureDiameter);
	}
	if (version>1)
	{
		ar & make_nvp("dofNear",a.dofNear);
		ar & make_nvp("dofFar",a.dofFar);
	}
}
#endif // !DONT_SERIALIZE_RRCAMERA

/*/------------------------------ CalculateParameters -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRSolver::CalculateParameters& a, const unsigned int version)
{
	ar & make_nvp("materialEmittanceMultiplier",a.materialEmittanceMultiplier);
	ar & make_nvp("materialEmittanceStaticQuality",a.materialEmittanceStaticQuality);
	ar & make_nvp("materialEmittanceVideoQuality",a.materialEmittanceVideoQuality);
	ar & make_nvp("materialEmittanceUsePointMaterials",a.materialEmittanceUsePointMaterials);
	ar & make_nvp("materialTransmittanceStaticQuality",a.materialTransmittanceStaticQuality);
	ar & make_nvp("materialTransmittanceVideoQuality",a.materialTransmittanceVideoQuality);
	ar & make_nvp("environmentStaticQuality",a.environmentStaticQuality);
	ar & make_nvp("environmentVideoQuality",a.environmentVideoQuality);
	ar & make_nvp("qualityIndirectDynamic",a.qualityIndirectDynamic);
	ar & make_nvp("qualityIndirectStatic",a.qualityIndirectStatic);
	ar & make_nvp("secondsBetweenDDI",a.secondsBetweenDDI);
}

//------------------------------ UpdateParameters -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRSolver::UpdateParameters& a, const unsigned int version)
{
	ar & make_nvp("lightMultiplier",a.lightMultiplier);
	ar & make_nvp("environmentMultiplier",a.environmentMultiplier);
	ar & make_nvp("useCurrentSolution",a.useCurrentSolution);
	ar & make_nvp("quality",a.quality);
	ar & make_nvp("qualityFactorRadiosity",a.qualityFactorRadiosity);
	ar & make_nvp("insideObjectsThreshold",a.insideObjectsThreshold);
	ar & make_nvp("rugDistance",a.rugDistance);
	ar & make_nvp("locality",a.locality);
	ar & make_nvp("aoIntensity",a.aoIntensity);
	ar & make_nvp("aoSize",a.aoSize);
	ar & make_nvp("measure_internal",a.measure_internal);
}

//------------------------------ FilteringParameters -------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRSolver::FilteringParameters& a, const unsigned int version)
{
	ar & make_nvp("smoothingAmount",a.smoothingAmount);
	ar & make_nvp("spreadForegroundColor",a.spreadForegroundColor);
	ar & make_nvp("backgroundColor",a.backgroundColor);
	ar & make_nvp("wrap",a.wrap);
}

//------------------------------ DateTime -----------------------------------

template<class Archive>
void serialize(Archive & ar, rr_gl::DateTime& a, const unsigned int version)
{
	a.tm_year += 1900;
	a.tm_mon += 1;
	ar & make_nvp("year",a.tm_year);
	ar & make_nvp("month",a.tm_mon); // 1..12
	ar & make_nvp("day",a.tm_mday); // 1..31
	ar & make_nvp("hour",a.tm_hour);
	ar & make_nvp("minute",a.tm_min);
	ar & make_nvp("second",a.tm_sec);
	if (version>0)
	{
		ar & make_nvp("nanosecond",a.tm_nsec);
	}
	a.tm_mon -= 1;
	a.tm_year -= 1900;
}
*/

//------------------------------ RRScene ------------------------------------

template<class Archive>
void serialize(Archive & ar, rr::RRScene& a, const unsigned int version)
{
	RR_ASSERT(g_serializationRuntime);
	g_serializationRuntime->nextBufferIsCube = false;
	ar & make_nvp("objects",a.objects);
	ar & make_nvp("lights",a.lights);
	g_serializationRuntime->nextBufferIsCube = true; // global is not thread safe, don't load two .rr3 at once
	ar & make_nvp("environment",prefix_buffer(a.environment)); postfix_buffer(Archive,a.environment);
	g_serializationRuntime->nextBufferIsCube = false;
	if (version>0)
	{
		ar & make_nvp("cameras",a.cameras);
	}
}

//---------------------------------------------------------------------------

} // namespace
} // namespace

//------------------------- runtime implemented --------------------------------

SerializationRuntime::SerializationRuntime(rr::RRFileLocator* _fileLocator, const char* _origin)
{
	textureLocator = _fileLocator;
	nextBufferIsCube = false;
	origin = _origin;
	if (g_serializationRuntime)
	{
		// this is potentially dangerous: it relies on boost.serialization being fully reentrant. maybe it is, but testing it is not easy
		rr::RRReporter::report(rr::INF2,"SerializationRuntime %s inside %s.\n",origin,g_serializationRuntime->origin);
	}
	backup = g_serializationRuntime;
	g_serializationRuntime = this;
}

SerializationRuntime::~SerializationRuntime()
{
	while (!bufferProxyInstances.empty())
		delete *bufferProxyInstances.begin();
	while (!meshProxyInstances.empty())
		delete *meshProxyInstances.begin();
	if (g_serializationRuntime!=this)
		// Scopes of runtimes must not overlap (new A, new B, delete A, delete B. <- this is wrong).
		rr::RRReporter::report(rr::ERRO,"SerializationRuntime %s scope overlaps.\n",origin);
	g_serializationRuntime = backup;
}

bool SerializationRuntime::exists()
{
	return g_serializationRuntime!=NULL;
}

//---------------------------------------------------------------------------

BOOST_SERIALIZATION_SPLIT_FREE(rr::RRMatrix3x4)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRString)
BOOST_SERIALIZATION_SPLIT_FREE(RRBufferProxy)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRMesh::Triangle)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRMeshArrays)
BOOST_SERIALIZATION_SPLIT_FREE(RRMeshProxy)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRSideBits)
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRObject)
#ifndef DONT_SERIALIZE_RRCAMERA
BOOST_SERIALIZATION_SPLIT_FREE(rr::RRCamera)
#endif

BOOST_CLASS_VERSION(rr::RRString,1)
BOOST_CLASS_VERSION(rr::RRMaterial,5)
#ifndef DONT_SERIALIZE_RRLIGHT
BOOST_CLASS_VERSION(rr::RRLight,5)
#endif
BOOST_CLASS_VERSION(RRMeshProxy,1) // this is actually RRMeshArrays version, load of RRMeshProxy only send it to load of RRMeshArrays
BOOST_CLASS_VERSION(rr::RRObject,2)
#ifndef DONT_SERIALIZE_RRCAMERA
BOOST_CLASS_VERSION(rr::RRCamera,3)
#endif
BOOST_CLASS_VERSION(rr::RRScene,1)

#endif
