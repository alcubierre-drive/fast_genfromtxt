#include "fast_genfromtxt.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>

#ifndef GENFROMTXT_GETLINE_MAX
#define GENFROMTXT_GETLINE_MAX 1048576
#endif

static int64_t genfromtxt_getline( char* line, FILE* stream ) {
    if (!line) return -1;
    fgets(line, GENFROMTXT_GETLINE_MAX-1, stream);
    return feof(stream) ? -1 : (int64_t)strlen(line);
}

static int iscomment( const char* line ) {
    while ((*line == ' ' || *line == '\t' || *line == '\n') && (*line)) line++;
    if (*line) return (*line) == '#';
    else return 0;
}

#define MAX(x,y) ((x) > (y) ? (x) : (y))

void* fast_genfromtxt_prepare( const char* fname, int64_t* nrow, int64_t* ncol ) {
    FILE* f = fopen(fname, "r");
    if (!f) { *nrow = -1, *ncol = -1; return NULL; }
    int64_t nread = 0,
            linenum = 0,
            ncols = 0;
    char line[GENFROMTXT_GETLINE_MAX] = {0};
    while ((nread = genfromtxt_getline(line, f)) != -1) {
        if (iscomment(line)) continue;
        linenum++;
        const char* separator = "\t ";
        int64_t n_found = 0;
        for (const char* p = line; *p; ) {
            p += strspn( p, separator );
            const char* prev = p;
            p += strcspn( p, separator );
            int64_t width = p - prev;
            if (width > 0) {
                n_found++;
            }
        }
        ncols = MAX(ncols, n_found);
    }
    *nrow = linenum;
    *ncol = ncols;
    rewind(f);
    return f;
}

void fast_genfromtxt( void* ptr, double* data ) {
    FILE* f = ptr;
    if (!f) { *data = DBL_MAX; return; }
    int64_t nread = 0,
            idx = 0;
    char line[GENFROMTXT_GETLINE_MAX] = {0};
    while ((nread = genfromtxt_getline(line, f)) != -1) {
        if (iscomment(line)) continue;
        const char* separator = "\t ";
        for (const char* p = line; *p; ) {
            p += strspn( p, separator );
            const char* prev = p;
            p += strcspn( p, separator );
            int64_t width = p - prev;
            if (width > 0) {
                char buf[width+1]; strncpy( buf, prev, width ); buf[width] = 0;
                data[idx++] = atof(buf);
            }
        }
    }
    fclose(f);
}

void fast_savetxt( const char* fname, const double* data, int64_t nrow, int64_t ncol, const char* header ) {
    FILE* f = fopen(fname, "w");
    if (!f) return;

    if (header)
        fprintf(f, "%s\n", header);

    int64_t idx = 0;
    for (int64_t r=0; r<nrow; ++r)
    for (int64_t c=0; c<ncol; ++c)
        fprintf(f, "%.11e%c", data[idx++], c==(ncol-1) ? '\n' : ' ');

    fclose(f);
}
