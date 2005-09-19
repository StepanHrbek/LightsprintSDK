
#include "miniball_config.h"

#ifdef MINIBALL_NO_STD_NAMESPACE
   #include <iostream.h>
#else
   #include <iostream>
#endif


namespace miniball 
{
   
   template <int d>
   class Wrapped_array {
       private:
           double coord [d];
   
       public:
           // default
           Wrapped_array()
           {}
   
           // copy from Wrapped_array
           Wrapped_array (const Wrapped_array& p)
           {
               for (int i=0; i<d; ++i)
                   coord[i] = p.coord[i];
           }
   
           // copy from double*
           Wrapped_array (const double* p)
           {
               for (int i=0; i<d; ++i)
                   coord[i] = p[i];
           }
   
           // assignment
           Wrapped_array& operator = (const Wrapped_array& p)
           {
               for (int i=0; i<d; ++i)
                   coord[i] = p.coord[i];
               return *this;
           }
   
           // coordinate access
           double& operator [] (int i)
           {
               return coord[i];
           }
           const double& operator [] (int i) const
           {
               return coord[i];
           }
           const double* begin() const
           {
               return coord;
           }
           const double* end() const
           {
               return coord+d;
           }
   };
   
   // Output
   
   #ifndef MINIBALL_NO_STD_NAMESPACE
       template <int d>
       std::ostream& operator << (std::ostream& os, const Wrapped_array<d>& p)
       {
           os << "(";
           for (int i=0; i<d-1; ++i)
               os << p[i] << ", ";
           os << p[d-1] << ")";
           return os;
       }
   #else
       template <int d>
       ostream& operator << (ostream& os, const Wrapped_array<d>& p)
       {
           os << "(";
           for (int i=0; i<d-1; ++i)
               os << p[i] << ", ";
           os << p[d-1] << ")";
           return os;
       }
   #endif
   
} // namespace
   

