#ifndef RRHASH_H
#define RRHASH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRHash.h
//! \brief LightsprintCore | hashing helper
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2011
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

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

		//! Default constructor keeps hash value uninitialized. For users who compute hash externally.
		RRHash() {}
		//! Calculates hash value from data.
		RRHash(const unsigned char* data, unsigned size);

		//! Creates filename from hash value.
		//! \param outputFilename
		//!  Buffer for returned filename.
		//! \param bufferSize
		//!  Size of buffer in bytes.
		//! \param version
		//!  Additional number melted into hash, different versions produce different filenames.
		//! \param prefix
		//!  Optional prefix (e.g. "c:/path/") of returned filename. If NULL, temporary directory is used.
		//! \param postfix
		//!  Optional postfix (e.g. ".extension") of returned filename. If NULL, no postfix is appended.
		void getFileName(char* outputFilename, unsigned bufferSize, unsigned version, const char* prefix, const char* postfix) const;

		bool operator !=(const RRHash& a) const;
	};

} // namespace

#endif
