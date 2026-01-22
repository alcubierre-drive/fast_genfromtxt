#pragma once

#include <stdint.h>

void* genfromtxt_serial_prepare( const char* fname, int64_t* nrow, int64_t* ncol );
void genfromtxt_serial( void* ptr, double* data );
void* genfromtxt_tmpfile_serial_prepare( const char* fname, int64_t* nrow, int64_t* ncol );
void genfromtxt_tmpfile_serial( void* ptr, double* data );
void savetxt_serial( const char* fname, const double* data, int64_t nrow, int64_t ncol, const char* header );
