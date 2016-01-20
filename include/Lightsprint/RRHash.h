//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRHash.h
//! \brief LightsprintCore | hashing helper
//----------------------------------------------------------------------------

#ifndef RRHASH_H
#define RRHASH_H

#include "RRMath.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRHash
	//
	//! Helper class that calculates/stores hash and converts it to filename.

	class RR_API RRHash
	{
	public:
		//! Hash value.
		unsigned char value[20];

		//! Resets value to 0.
		RRHash();
		//! Calculates hash value from data.
		RRHash(const unsigned char* data, unsigned size);

		//! Returns filename created from hash value, in format prefix + hash + postfix.
		//! \param version
		//!  Additional number melted into hash, different versions produce different filenames.
		//! \param prefix
		//!  Optional prefix (e.g. "c:/path/") of returned filename. If nullptr, temporary directory is used.
		//! \param postfix
		//!  Optional postfix (e.g. ".extension") of returned filename. If nullptr, no postfix is appended.
		RRString getFileName(unsigned version, const char* prefix, const char* postfix) const;

		bool operator !=(const RRHash& a) const;
		void operator +=(const RRHash& a);
	};

} // namespace

#endif
