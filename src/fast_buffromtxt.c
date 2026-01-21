#include "fast_genfromtxt.h"
#include "vector.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <string.h>
#include <ctype.h>
#include <omp.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX(A,B) ((A) > (B) ? (A) : (B))
#define MIN(A,B) ((A) < (B) ? (A) : (B))

typedef struct { VECTOR_DECL( double, v ) } vec_double_t;

typedef struct {
    char* bytes;
    int64_t nbytes;
    int nthr;

    int64_t* displ;
    int64_t* count;
    vec_double_t* results;

    int64_t* nrow;
    int64_t* ncol;
} fast_buffromtxt_t;

typedef struct {
    char* buf;
    int64_t offset;
    int64_t sz;
} virtual_file_t;

typedef struct {
    char* str;
    int64_t sz;
} virtual_getline_t;

static inline virtual_getline_t virtual_getline( virtual_file_t* file ) {
    virtual_getline_t result = { .str=NULL, .sz=0 };
    if (file->offset >= file->sz) return result;
    // skip leading zeros
    while (file->offset < file->sz && !file->buf[file->offset]) file->offset++;
    result.str = file->buf + file->offset;
    const int64_t n = file->sz - file->offset;
    for (int64_t i=0; i<n; ++i) {
        result.sz++;
        if (result.str[i] == '\n') {
            result.str[i] = '\0';
            break;
        }
    }
    file->offset += result.sz;
    return result;
}

double* fast_buffromtxt( const char* fname, int64_t* nrow, int64_t* ncol, int nthr ) {
    // TODO
    // printf( "setup…\n" );
    if (nthr <= 0) nthr = omp_get_max_threads();

    // have this here to return it on error
    *nrow = *ncol = -1;
    VECTOR_DECL( double, result )
    result = NULL;
    result_sz = result_cap = 0;
    fast_buffromtxt_t fb = {0};
    fb.nthr = nthr;

    int fd = open(fname, O_RDONLY);
    if (fd <= 0) goto cleanup;

    struct stat finfo = {0};
    stat(fname, &finfo);
    fstat(fd, &finfo);
    fb.nbytes = finfo.st_size;
    fb.bytes = malloc(fb.nbytes+1);
    if (!fb.bytes) { close(fd); goto cleanup; }

    // TODO
    // printf( "read file…\n" );
    char* buf = fb.bytes;
    while (1) {
        ssize_t nread = read(fd, buf, fb.nbytes);
        if (nread <= 0) break;
        buf += nread;
    }
    fb.bytes[fb.nbytes] = 0;
    close(fd);

    // TODO
    // printf( "alloc structures…\n" );
    // set up initial displacements and counts
    fb.displ = calloc(fb.nthr+1, sizeof*fb.displ);
    fb.count = calloc(fb.nthr, sizeof*fb.displ);
    fb.nrow = calloc(fb.nthr, sizeof*fb.nrow);
    fb.ncol = calloc(fb.nthr, sizeof*fb.ncol);
    fb.results = calloc(fb.nthr, sizeof*fb.results);
    if (!fb.displ || !fb.count || !fb.nrow || !fb.ncol || !fb.results) goto cleanup;
    // counts
    for (int t=0; t<nthr; ++t) fb.count[t] = fb.nbytes / fb.nthr;
    for (int b=0; b<fb.nbytes%fb.nthr; ++b) fb.count[b]++;
    // displs
    for (int t=1; t<fb.nthr; ++t) fb.displ[t] = fb.displ[t-1] + fb.count[t-1];
    fb.displ[fb.nthr] = fb.nbytes; // that makes things easier

    // // TODO
    // for (int t=0; t<fb.nthr; ++t)
    //     printf( "%li->%li\n", fb.displ[t], fb.displ[t]+fb.count[t] );


    // TODO
    // printf( "find newlines…\n" );
    // find closest (to left) newline and update corresponding displacement
    #pragma omp parallel for num_threads(fb.nthr)
    for (int t=0; t<fb.nthr; ++t) {
        while (fb.displ[t] > 0) {
            if (fb.bytes[--fb.displ[t]] == '\n') {
                fb.displ[t]++;
                break;
            }
        }
    }

    // update counts
    for (int t=0; t<fb.nthr; ++t) fb.count[t] = fb.displ[t+1] - fb.displ[t];

    // // TODO
    // for (int t=0; t<fb.nthr; ++t)
    //     printf( "%li->%li\n", fb.displ[t], fb.displ[t]+fb.count[t] );

    // TODO
    // printf( "read data…\n" );
    int64_t n_comment_lines = 0;
    // get the data on each thread
    #pragma omp parallel for num_threads(fb.nthr)
    for (int t=0; t<fb.nthr; ++t) {
        int64_t n_comment_lines_thr = 0;
        int64_t count = fb.count[t];
        // terminate the bytes appropriately (use extra byte allocated up top)
        fb.bytes[fb.displ[t] + (t == fb.nthr-1) ? count : (count-1)] = '\0';

        if (count) {
            int64_t ncols_max = INT64_MIN,
                    ncols_min = INT64_MAX;
            // this should be enough space
            VECTOR_RESERVE( fb.results[t].v, fb.nbytes / fb.nthr );
            virtual_file_t vfile = {.buf = fb.bytes + fb.displ[t],
                                    .sz = count,
                                    .offset = 0};
            virtual_getline_t vline = {0};
            do {
                vline = virtual_getline( &vfile );
                if (vline.str) {
                    char* buf = vline.str;
                    while (*buf) { if (isspace(*buf)) buf++; else break; }
                    if (!*buf || *buf == '#') {
                        n_comment_lines_thr++;
                    } else {
                        long nadvance = 0;
                        double number = 0;
                        int64_t ncol_current = 0;
                        while (1) {
                            int nread = sscanf( vline.str, "%le%ln", &number, &nadvance );
                            if (nread == 1) {
                                vline.str += nadvance;
                                ncol_current++;
                                VECTOR_PUSH_BACK( fb.results[t].v, number );
                            } else {
                                break;
                            }
                        }
                        ncols_max = MAX(ncol_current, ncols_max);
                        ncols_min = MIN(ncol_current, ncols_min);
                        fb.ncol[t] = ncol_current;
                        fb.nrow[t]++;
                    }
                }
            } while (vline.str != NULL);
            VECTOR_SHRINK_TO_FIT( fb.results[t].v );

            // silent error mitigation
            if (ncols_max != ncols_min) {
                fprintf( stderr, "error (thr#%i): #cols=%li..%li. skipping %li rows.\n",
                         t, ncols_min, ncols_max, fb.nrow[t] );
                fb.nrow[t] = 0;
            }
        }

        #pragma omp atomic
        n_comment_lines += n_comment_lines_thr;
    }

    // TODO
    // printf( "output data…\n" );
    *nrow = fb.nrow[0];
    *ncol = fb.ncol[0];
    for (int t=1; t<fb.nthr; ++t) {
        if (fb.ncol[t] != fb.ncol[0]) {
            fprintf( stderr, "error (thr#%i): #cols=%li, should be %li. skipping %li rows.\n",
                     t, fb.ncol[t], fb.ncol[0], fb.nrow[t] );
            fb.nrow[t] = 0;
        }
        *nrow += fb.nrow[t];
    }

    VECTOR_COPY( result, fb.results[0].v );
    VECTOR_RESERVE( result, (*nrow)*(*ncol) );
    for (int t=1; t<fb.nthr; ++t) {
        if (fb.nrow[t] > 0) {
            memcpy( result+result_sz, fb.results[t].v, sizeof(double) * fb.results[t].v_sz );
            result_sz += fb.results[t].v_sz;
        }
        VECTOR_FREE( fb.results[t].v );
    }
    VECTOR_SHRINK_TO_FIT( result );

cleanup:
    munmap(fb.bytes, fb.nbytes);
    free(fb.displ);
    free(fb.count);
    free(fb.results);
    free(fb.nrow);
    free(fb.ncol);
    return result; // should be NULL on error
}

void fast_buffromtxt_free( void* ptr ) { free( ptr ); }

/*
#include <time.h>
static inline double wtime( void ) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.e+9;
}
int main() {
    int64_t nrow, ncol;
    double* ary;
    double tick, tock;

    tick = wtime();
    ary = fast_buffromtxt( "RAND.dat", &nrow, &ncol, -1 );
    free( ary );
    tock = wtime();
    printf( "buf: %.2f (%li×%li)\n", tock-tick, nrow, ncol );

    tick = wtime();
    void* handle = fast_genfromtxt_prepare( "RAND.dat", &nrow, &ncol );
    ary = malloc( sizeof*ary * nrow*ncol );
    fast_genfromtxt( handle, ary );
    free( ary );
    tock = wtime();
    printf( "gen: %.2f (%li×%li)\n", tock-tick, nrow, ncol );
}
*/
