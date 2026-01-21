#pragma once

#include <stdint.h>

#ifndef VECTOR_DECL
#define VECTOR_DECL( type, ary ) \
    int64_t ary##_cap; \
    int64_t ary##_sz; \
    type* ary;
#endif

#ifndef VECTOR_DECL_INIT_CAP
#define VECTOR_DECL_INIT_CAP( type, ary, cap ) { \
    ary##_cap = (cap); \
    ary##_sz = 0; \
    ary = (type*)calloc(ary##_cap, sizeof(type)); \
}
#endif

#ifndef VECTOR_INIT_CAP
#define VECTOR_INIT_CAP( type, ary, cap ) \
    VECTOR_DECL( type, ary ); \
    VECTOR_DECL_INIT_CAP( type, ary, cap );
#endif

#ifndef VECTOR_DEFAULT_CAP
#define VECTOR_DEFAULT_CAP 1024
#endif

#ifndef VECTOR_DECL_INIT
#define VECTOR_DECL_INIT( type, ary ) VECTOR_DECL_INIT_CAP( type, ary, VECTOR_DEFAULT_CAP )
#endif

#ifndef VECTOR_INIT
#define VECTOR_INIT( type, ary ) VECTOR_INIT_CAP( type, ary, VECTOR_DEFAULT_CAP )
#endif

#ifndef VECTOR_PUSH_BACK
#define VECTOR_PUSH_BACK( ary, ... ) { \
    if (ary##_sz >= ary##_cap) { \
        void** p_ary = (void**)&ary; \
        *p_ary = realloc(ary, sizeof(*ary) * ((ary##_cap <= 0) ? (ary##_cap = 1) : (ary##_cap *= 2))); \
    } \
    ary[ ary##_sz++ ] = __VA_ARGS__; \
}
#endif

#ifndef VECTOR_RESERVE
#define VECTOR_RESERVE( ary, cap ) { \
    void** p_ary = (void**)&ary; \
    *p_ary = realloc((ary), sizeof(*ary) * (cap)); \
    ary##_cap = (cap); \
}
#endif

#ifndef VECTOR_RESIZE
#define VECTOR_RESIZE( ary, sz ) { \
    if ((sz) != ary##_sz) { VECTOR_RESERVE( ary, sz ); } \
    ary##_sz = (sz); \
}
#endif

#ifndef VECTOR_SHRINK_TO_FIT
#define VECTOR_SHRINK_TO_FIT( ary ) VECTOR_RESIZE( ary, ary##_sz )
#endif

#ifndef VECTOR_FREE
#define VECTOR_FREE( ary ) free( ary )
#endif

#ifndef VECTOR_COPY
#define VECTOR_COPY( a, b ) { \
    a = b; \
    a##_sz = b##_sz; \
    a##_cap = b##_cap; \
}
#endif
