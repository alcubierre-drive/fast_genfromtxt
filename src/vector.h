// a shadow memory implementation of dynamic arrays based on malloc/realloc,
// wrapped into macros to be type agnostic. aligned to 512bit for avx extensions
// on vector's data.
//
// usage in functions works, also when passed as "pointer to array", e.g.,
//
// void do_complicated_thing(int** vector, ...) {
//      ptrvec_resize(*vector, 10);
//      ptrvec_push(*vector, -1);
//      ptrvec_push(*vector, -1);
//      ptrvec_push(*vector, -1);
//      for (int i=0; i<ptrvec_sz(*vector); ++i) {
//      }
//      ptrvec_free(*vector);
// }

#pragma once

#include <stdint.h>
#include <stdlib.h>

// make this 512bit padded for alignment of vector's memory
#ifndef DV_VEC_HEADER_PADDING_NBYTES
#define DV_VEC_HEADER_PADDING_NBYTES() (sizeof(int64_t)*(2+4))
#endif

#ifndef DV_VEC_INITIAL_CAP
#define DV_VEC_INITIAL_CAP 128
#endif

typedef struct {
    int64_t sz, cap;
    #if DV_VEC_HEADER_PADDING_NBYTES != 0
    char _padding[DV_VEC_HEADER_PADDING_NBYTES];
    #endif
} _ptrvec_header_t;

#define _ptrvec_get_header(ptr) ((ptr) ? (_ptrvec_header_t*)(ptr)-1 : NULL)

#define ptrvec_sz(ptr)  (_ptrvec_get_header(ptr) ? _ptrvec_get_header(ptr)->sz  : 0)
#define ptrvec_cap(ptr) (_ptrvec_get_header(ptr) ? _ptrvec_get_header(ptr)->cap : 0)

#define ptrvec_init_cap(ptr, CAP) do { \
    _ptrvec_header_t* _header = calloc(1, sizeof*_header + sizeof*ptr * CAP); \
    _header->cap = CAP; \
    ptr = (void*)(_header + 1); \
} while (0)

#define ptrvec_init(ptr) ptrvec_init_cap(ptr, DV_VEC_INITIAL_CAP)

#define ptrvec_push(ptr, ...) do { \
    _ptrvec_header_t* _header = _ptrvec_get_header(ptr); \
    if (!_header) { \
        ptrvec_init(ptr); \
        _header = _ptrvec_get_header(ptr); \
    } else if (_header->sz >= _header->cap) { \
        _header->cap *= 2; \
        _header = realloc(_header, sizeof*_header + sizeof*ptr * _header->cap); \
        ptr = (void*)(_header+1); \
    } \
    ptr[_header->sz++] = __VA_ARGS__; \
} while (0)

#define ptrvec_reserve(ptr, CAP) do { \
    if ((CAP) == 0) { \
        ptrvec_free(ptr); \
        (ptr) = NULL; \
    } else { \
        _ptrvec_header_t* _header = _ptrvec_get_header(ptr); \
        if (!_header) { \
            ptrvec_init_cap(ptr, CAP); \
            _header = _ptrvec_get_header(ptr); \
        } else { \
            _header->cap = CAP; \
            _header = realloc(_header, sizeof*_header + sizeof*ptr * _header->cap); \
            ptr = (void*)(_header+1); \
        } \
    } \
} while (0)

#define ptrvec_resize_lazy(ptr, SZ) do { \
    _ptrvec_header_t* _header = _ptrvec_get_header(ptr); \
    if (_header) { \
        _header->sz = SZ; \
    } \
} while (0)

#define ptrvec_resize(ptr, SZ) do { \
    ptrvec_reserve(ptr, SZ); \
    _ptrvec_header_t* _header = _ptrvec_get_header(ptr); \
    if (_header) { \
        _header->sz = (SZ); \
    } \
} while (0)

#define ptrvec_shrink(ptr) do { \
    _ptrvec_header_t* _header = _ptrvec_get_header(ptr); \
    if (_header) { \
        _header->cap = _header->sz; \
        _header = realloc(_header, sizeof*_header + sizeof*ptr * _header->sz); \
        if (!_header) { \
            (ptr) = NULL; \
        } else { \
            ptr = (void*)(_header+1); \
        } \
    } \
} while (0)

#define ptrvec_free(ptr) free(_ptrvec_get_header(ptr))
