/*
     Copyright (c) 2008-2009 NetAllied Systems GmbH
 
     This file is part of COLLADABaseUtils.
 
     Licensed under the MIT Open Source License, 
     for details please see LICENSE file or the website
     http://www.opensource.org/licenses/mit-license.php
 */

#ifndef __COLLADABU_HASH_MAP_H__
#define __COLLADABU_HASH_MAP_H__

#include "COLLADABUPrerequisites.h"
#include "COLLADABUPlatform.h"
#define COLLADABU_HAVE_TR1_UNORDERED_MAP
#ifndef WIN32
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3)
#undef COLLADABU_HAVE_TR1_UNORDERED_MAP
#else
  #include <tr1/unordered_map>
  #include <tr1/unordered_set>
#endif
#else
#  undef COLLADABU_HAVE_TR1_UNORDERED_MAP
#  if defined(_MSC_VER) && (_MSC_VER >= 1600) \
   && defined(_MSC_FULL_VER) && \
   !defined(__SGI_STL_PORT) && \
   !defined(_STLPORT_VERSION) && \
   !defined(_RWSTD_VER_STR) && \
   !defined(_RWSTD_VER)
#    define COLLADABU_HAVE_TR1_UNORDERED_MAP
#    include <unordered_map>
#    include <unordered_set>
#  endif
#endif
#ifndef COLLADABU_HAVE_TR1_UNORDERED_MAP
#  if defined(COLLADABU_OS_LINUX) || defined(COLLADABU_OS_MAC)
#	include <ext/hash_map>
#	include <ext/hash_set>
#  else
#	include <hash_map>
#	include <hash_set>
#  endif
#endif
// file to include the hash map platform independent

namespace COLLADABU
{


#ifdef COLLADABU_HAVE_TR1_UNORDERED_MAP
    using namespace std::tr1;
    template<class X, class Y > class hash_map:public std::tr1::unordered_map<X,Y>
    { public:
      hash_map(){}
      hash_map(const hash_map&a):std::tr1::unordered_map<X,Y>(a){}
      hash_map&operator=(const hash_map&a){std::tr1::unordered_map<X,Y>::operator=(*this,a);return this;}
    };
#else
#  if defined(COLLADABU_OS_LINUX) || defined(COLLADABU_OS_MAC)
        using namespace __gnu_cxx;
#  else
        using namespace stdext;
#  endif
#endif


} // namespace COLLADABU

#endif // __COLLADABU_HASH_MAP_H__
