/*
    Copyright (c) 2008-2009 NetAllied Systems GmbH

	This file is part of COLLADABaseUtils.
	
    Licensed under the MIT Open Source License, 
    for details please see LICENSE file or the website
    http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __COLLADABU_PLATTFORM_H__
#define __COLLADABU_PLATTFORM_H__


#if (defined(__APPLE__) || defined(OSMac_)) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__)) || defined(__APPLE_CC__) 
#  define COLLADABU_OS_MAC
#  ifdef __LP64__
#    define COLLADABU_OS_MAC64
#  else
#    define COLLADABU_OS_MAC32
#  endif
#elif (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#  define COLLADABU_OS_WIN64
#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))
#  define COLLADABU_OS_WIN32
#elif defined(__linux__) || defined(__linux)
#  define COLLADABU_OS_LINUX
#endif

#if defined(COLLADABU_OS_WIN32) || defined(COLLADABU_OS_WIN64)
#  define COLLADABU_OS_WIN
#endif


#endif //__COLLADABU_PLATTFORM_H__
