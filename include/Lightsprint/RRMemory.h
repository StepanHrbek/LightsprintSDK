#ifndef RRMEMORY_H
#define RRMEMORY_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMemory.h
//! \brief LightsprintCore | base elements related to memory allocation
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2008
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <new> // operators new/delete
#include "RRMath.h"

#ifndef SAFE_DELETE
#define SAFE_DELETE(a)       {delete a;a=NULL;}
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(a) {delete[] a;a=NULL;}
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


} // namespace

#endif
