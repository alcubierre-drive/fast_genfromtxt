#pragma once

#include <stdint.h>

void* fast_genfromtxt_prepare( const char* fname, int64_t* nrow, int64_t* ncol );
void fast_genfromtxt( void* ptr, double* data );

void* fast_cachefromtxt_prepare( const char* fname, int64_t* nrow, int64_t* ncol );
void fast_cachefromtxt( void* ptr, double* data );

void fast_savetxt( const char* fname, const double* data, int64_t nrow, int64_t ncol, const char* header );
