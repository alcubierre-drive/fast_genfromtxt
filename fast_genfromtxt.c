#include "fast_genfromtxt.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>

#ifndef GENFROMTXT_GETLINE_MAX
#define GENFROMTXT_GETLINE_MAX 8192
#endif

static int genfromtxt_getline( char* line, FILE* stream ) {
    if (!line) return -1;
    fgets(line, GENFROMTXT_GETLINE_MAX-1, stream);
    return feof(stream) ? -1 : strlen(line);
}

static int iscomment( const char* line ) {
    while ((*line == ' ' || *line == '\t' || *line == '\n') && (*line)) line++;
    if (*line) return (*line) == '#';
    else return 0;
}

#define MAX(x,y) ((x) > (y) ? (x) : (y))

void fast_genfromtxt_prepare( const char* fname, int* nrow, int* ncol ) {
    FILE* f = fopen(fname, "r");
    if (!f) { *nrow = -1, *ncol = -1; return; }
    int nread = 0,
        linenum = 0,
        ncols = 0;
    char line[GENFROMTXT_GETLINE_MAX] = {0};
    while ((nread = genfromtxt_getline(line, f)) != -1) {
        if (iscomment(line)) continue;
        linenum++;
        const char* separator = "\t ";
        char buf[GENFROMTXT_GETLINE_MAX] = {0};
        int n_found = 0;
        for (const char* p = line; *p; ) {
            p += strspn( p, separator );
            const char* prev = p;
            p += strcspn( p, separator );
            int width = p - prev;
            if (width) {
                buf[width] = 0;
                n_found++;
            }
        }
        ncols = MAX(ncols, n_found);
    }
    *nrow = linenum;
    *ncol = ncols;
    fclose(f);
}

void fast_genfromtxt( const char* fname, double* data ) {
    FILE* f = fopen(fname, "r");
    if (!f) { *data = DBL_MAX; return; }
    int nread = 0,
        idx = 0;
    char line[GENFROMTXT_GETLINE_MAX] = {0};
    while ((nread = genfromtxt_getline(line, f)) != -1) {
        if (iscomment(line)) continue;
        const char* separator = "\t ";
        char buf[GENFROMTXT_GETLINE_MAX] = {0};
        for (const char* p = line; *p; ) {
            p += strspn( p, separator );
            const char* prev = p;
            p += strcspn( p, separator );
            int width = p - prev;
            if (width) {
                memcpy( buf, prev, width ); buf[width] = 0;
                data[idx++] = atof(buf);
            }
        }
    }
    fclose(f);
}
