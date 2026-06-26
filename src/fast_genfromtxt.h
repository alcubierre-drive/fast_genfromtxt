#pragma once

#include <stdint.h>

double* fast_genfromtxt_mmap( const char* fname, int64_t* nrow, int64_t* ncol, int nthr );
double* fast_genfromtxt_mmap_serial( const char* fname, int64_t* nrow, int64_t* ncol );
void fast_genfromtxt_free( double* fg_buf );

void fast_savetxt_buffered( const char* fname, const double* data, int64_t nrow,
                       int64_t ncol, const char* header, int nthr );
void fast_savetxt_serial( const char* fname, const double* data, int64_t nrow,
                       int64_t ncol, const char* header );
