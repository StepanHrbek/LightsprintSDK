#ifndef COLLIDER_STDINT_H
#define COLLIDER_STDINT_H

// temporary replacement for ISO C99 stdint.h missing in Visual C++

typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

typedef signed __int8  int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;

static union
{
    char    int8_t_incorrect[sizeof(  int8_t) == 1];
    char   uint8_t_incorrect[sizeof( uint8_t) == 1];
    
    char   int16_t_incorrect[sizeof( int16_t) == 2];
    char  uint16_t_incorrect[sizeof(uint16_t) == 2];
    
    char   int32_t_incorrect[sizeof( int32_t) == 4];
    char  uint32_t_incorrect[sizeof(uint32_t) == 4];
};

#endif
