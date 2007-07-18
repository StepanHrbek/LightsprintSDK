#ifndef RRMEMORY_H
#define RRMEMORY_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMemory.h
//! \brief Base elements related to memory allocation.
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <new> // operators new/delete
#include "RRMath.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRUniformlyAllocated
	//! Base class for objects allocated on our heap.
	//
	//! Allocating on our heap prevents corruption
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
	//! Base class for objects aligned in memory for SIMD instructions.
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


} // namespace

#endif
