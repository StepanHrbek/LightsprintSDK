#ifndef RRMEMORY_H
#define RRMEMORY_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMemory.h
//! \brief LightsprintCore | base elements related to memory allocation
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2011
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <new> // operators new/delete
#include "RRMath.h"

// Common memory macros.
#define RR_SAFE_FREE(a)         {free(a);a=NULL;}
#define RR_SAFE_DELETE(a)       {delete a;a=NULL;}
#define RR_SAFE_DELETE_ARRAY(a) {delete[] a;a=NULL;}
#define RR_SAFE_RELEASE(a)      {if(a){(a)->Release();a=NULL;}}
#if defined(__BIG_ENDIAN__) || defined(__PPC__) // || defined(XBOX)
#define RR_BIG_ENDIAN
#endif

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRUniformlyAllocated
	//! When used as base class, delete works correctly without regard who calls it.
	//
	//! Ensures that delete doesn't corrupt heap
	//! in environment with multiple heaps.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRUniformlyAllocated
	{
	public:
		//! Allocates aligned space for instance of any derived class.
		void* operator new(std::size_t n);
		//! Allocates aligned space for array of instances of any derived class.
		void* operator new[](std::size_t n);
		//! Frees aligned space allocated by new.
		void operator delete(void* p, std::size_t n);
		//! Frees aligned space allocated by new[].
		void operator delete[](void* p, std::size_t n);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRAligned
	//! When used as base class, objects are 16byte aligned for SIMD instructions.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRAligned : public RRUniformlyAllocated
	{
	public:
		//! Allocates aligned space for instance of any derived class.
		void* operator new(std::size_t n);
		//! Allocates aligned space for array of instances of any derived class.
		void* operator new[](std::size_t n);
		//! Frees aligned space allocated by new.
		void operator delete(void* p, std::size_t n);
		//! Frees aligned space allocated by new[].
		void operator delete[](void* p, std::size_t n);
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRNonCopyable
	//! When used as base class, object copying is not allowed.
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Both uniformly allocated and non copyable, without sideeffects of multiple inheritance.
	class RR_API RRUniformlyAllocatedNonCopyable : public RRUniformlyAllocated
	{
	protected:
		RRUniformlyAllocatedNonCopyable() {}
		~RRUniformlyAllocatedNonCopyable() {}
	private:
		RRUniformlyAllocatedNonCopyable(const RRUniformlyAllocatedNonCopyable&);
		const RRUniformlyAllocatedNonCopyable& operator=(const RRUniformlyAllocatedNonCopyable&);
	};

	//! Both aligned and non copyable, without sideeffects of multiple inheritance.
	class RR_API RRAlignedNonCopyable : public RRAligned
	{
	protected:
		RRAlignedNonCopyable() {}
		~RRAlignedNonCopyable() {}
	private:
		RRAlignedNonCopyable(const RRAlignedNonCopyable&);
		const RRAlignedNonCopyable& operator=(const RRAlignedNonCopyable&);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Minimalistic string, for portable API.
	//
	//! RRString is intentionally minimalistic, for passing data, not for manipulation.
	//! Its only purpose is to replace std::[w]string in public Lightsprint headers,
	//! to make SDK compatible with any STL implementation.
	//!
	//! Encoding:
	//! - char* is always null terminated string in local charset
	//! - wchar_t* is always null terminated string in UTF16 (windows) or UTF32 (mac, linux)
	//!
	//! RRString implicitly converts from and explicitly to char* and wchar_t*.
	//! Conversions from/to third party string types are explicit, using RR_* macros.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRString : public RRUniformlyAllocated
	{
	public:
		// modifiers
		RRString();
		RRString(const RRString& a);
		RRString(const char* a);
		RRString(const wchar_t* a);
		void clear();
		RRString& operator =(const RRString& a);
		RRString& operator =(const char* a);
		RRString& operator =(const wchar_t* a);

		// comparators
		bool operator ==(const RRString& a) const;
		bool operator ==(const char* a) const;
		bool operator ==(const wchar_t* a) const;
		bool operator !=(const RRString& a) const;
		bool operator !=(const char* a) const;
		bool operator !=(const wchar_t* a) const;
		
		// accessors
		const char* c_str() const {return str?str:"";} ///< Never returns NULL, empty string is "".
		const wchar_t* w_str() const {return wstr?wstr:L"";} ///< Never returns NULL, empty string is L"".
		bool empty() const {return str==NULL;}

		~RRString();
		void _skipDestructor(); ///< For internal use only.
	private:
		char* str; ///< Never "", empty string is NULL.
		wchar_t* wstr; ///< Never "", empty string is NULL.
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// String conversion macros.
	//
	// RR     - RRString
	// PATH   - boost::filesystem:path
	// STD    - std::string
	// STDW   - std::wstring
	// WX     - wxWidgets
	// STREAM - fstream constructor
	// CHAR   - char*
	//
	// Convert strings with these macros to
	// - ensure unicode correctness
	// - keep track of all suspicious conversions, make them searchable
	//
	// Macros only select suitable accessor so that implicit conversion can take place,
	// they often don't return destination type.
	//
	//////////////////////////////////////////////////////////////////////////////

	#define RR_RR2PATH(r)   (r).w_str()           // ok, unicode->unicode
	#define RR_PATH2RR(p)   (p).wstring().c_str() // ok, unicode->unicode
	#define RR_RR2STD(s)    (r).c_str()           // WRONG, unicode->local
	#define RR_STD2RR(s)    (s).c_str()           // suspicious, local->unicode
	#define RR_RR2STDW(s)   (r).w_str()           // ok, unicode->unicode
	#define RR_STDW2RR(s)   (s).c_str()           // ok, unicode->unicode
	#define RR_WX2RR(w)     ((const wchar_t*)(w)) // ok, unicode->unicode
	#define RR_WX2PATH(w)   ((const wchar_t*)(w)) // ok, unicode->unicode
	#define RR_RR2WX(r)     ((r).w_str())         // ok, unicode->unicode
	#define RR_PATH2WX(p)   ((p).wstring())       // ok, unicode->unicode
#ifdef _WIN32
	#define RR_WX2STREAM(w) ((const wchar_t*)(w)) // ok, unicode->unicode
#else
	#define RR_WX2STREAM(w) ((const char*)(w))    // WRONG, unicode->local
#endif
	#define RR_WX2CHAR(w)   ((const char*)(w))    // WRONG, unicode->local

} // namespace

#endif
