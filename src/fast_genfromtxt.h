#pragma once

#include <stdint.h>

void* fast_genfromtxt_prepare( const char* fname, int64_t* nrow, int64_t* ncol );
void fast_genfromtxt( void* ptr, double* data );

void* fast_tmpfromtxt_prepare( const char* fname, int64_t* nrow, int64_t* ncol );
void fast_tmpfromtxt( void* ptr, double* data );

double* fast_buffromtxt( const char* fname, int64_t* nrow, int64_t* ncol, int nthr );
void fast_buffromtxt_free( void* ptr );

void fast_savetxt( const char* fname, const double* data, int64_t nrow, int64_t ncol, const char* header );
