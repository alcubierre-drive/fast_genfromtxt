#pragma once

#include <stdint.h>

double* genfromtxt_buffered( const char* fname, int64_t* nrow, int64_t* ncol, int nthr );
double* genfromtxt_mmap( const char* fname, int64_t* nrow, int64_t* ncol, int nthr );
void genfromtxt_buffered_free( void* ptr );

void savetxt_buffered( const char* fname, const double* data, int64_t nrow,
                       int64_t ncol, const char* header, int nthr );
