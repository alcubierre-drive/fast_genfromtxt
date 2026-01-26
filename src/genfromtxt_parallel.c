#include "genfromtxt_parallel.h"
#include "vector.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <omp.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX(A,B) ((A) > (B) ? (A) : (B))
#define MIN(A,B) ((A) < (B) ? (A) : (B))

static inline int64_t* count_displ( int64_t num, int64_t par ) {
    int64_t* count = calloc(2*par, sizeof*count);
    int64_t* displ = count + par;

    for (int64_t p=0; p<par; ++p) count[p] = num/par;
    for (int64_t p=0; p<num%par; ++p) count[p]++;

    for (int64_t p=1; p<par; ++p) displ[p] = displ[p-1]+count[p-1];
    return count;
}

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
} genfromtxt_buffered_t;

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

double* genfromtxt_buffered( const char* fname, int64_t* nrow, int64_t* ncol, int nthr ) {
    // TODO
    // printf( "setup…\n" );
    if (nthr <= 0) nthr = omp_get_max_threads();

    // have this here to return it on error
    *nrow = *ncol = -1;
    VECTOR_DECL( double, result )
    result = NULL;
    result_sz = result_cap = 0;
    genfromtxt_buffered_t fb = {0};
    fb.nthr = nthr;

    int fd = open(fname, O_RDONLY);
    if (fd <= 0) goto cleanup;

    struct stat finfo = {0};
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

        if (fb.count[t]) {
            int64_t ncols_max = INT64_MIN,
                    ncols_min = INT64_MAX;
            // this should be enough space
            VECTOR_RESERVE( fb.results[t].v, fb.nbytes / fb.nthr );
            virtual_file_t vfile = {.buf = fb.bytes + fb.displ[t],
                                    .sz = fb.count[t],
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
    free(fb.bytes);
    free(fb.displ);
    free(fb.count);
    free(fb.results);
    free(fb.nrow);
    free(fb.ncol);
    return result; // should be NULL on error
}

void genfromtxt_buffered_free( void* ptr ) { free( ptr ); }

typedef struct {
    int64_t offset;
    int line_continues;
    int buffer_continues;
    int has_number;
} column_t;

column_t get_column( char* restrict buffer, int64_t offset, int64_t size, char* restrict number ) {
    column_t result = {.offset=offset, .line_continues=1, .buffer_continues=1, .has_number=0};

    while (isspace(buffer[result.offset]) && result.offset < size)
        result.offset++;

    int nnumber = 0;
    int iscomment = 0;
    while (!isspace(buffer[result.offset]) && result.offset < size) {
        if ((number[nnumber++] = buffer[result.offset++]) == '#') {
            iscomment = 1;
            nnumber--;
            break;
        }
    }
    result.has_number = (nnumber > 0);
    number[nnumber++] = '\0';
    assert( nnumber < 64 );

    if (iscomment) {
        while (buffer[result.offset] != '\n' && result.offset < size)
            result.offset++;
        result.line_continues = 0;
    } else {
        while (isspace(buffer[result.offset]) && result.offset < size) {
            if (buffer[result.offset++] == '\n') {
                result.line_continues = 0;
                break;
            }
        }
    }
    result.buffer_continues = (result.offset < size);

    return result;
}

#ifdef USE_DOUBLE_CONVERSION
#include "double_conversion_wrap.h"
#endif

double* genfromtxt_mmap( const char* fname, int64_t* nrow, int64_t* ncol, int nthr ) {
    if (nthr <= 0) nthr = omp_get_max_threads();

    // have this here to return it on error
    *nrow = *ncol = -1;

    int fd = open(fname, O_RDONLY);
    char* bytes = NULL;
    int64_t nbytes = 0;
    int64_t* count = NULL;
    vec_double_t* results = NULL;
    VECTOR_DECL( double, result );
    result = NULL; result_sz = result_cap = 0;

    if (fd <= 0) goto mclean;

    struct stat finfo = {0};
    fstat(fd, &finfo);
    nbytes = finfo.st_size;
    if ((bytes = mmap(NULL, nbytes, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        close(fd);
        goto mclean;
    }
    close(fd);

    int nthr_prev = nthr;
    while (nthr > 0) {
        if (nbytes / nthr < 128) nthr--;
        else break;
    }
    nthr = MAX(nthr, 0);
    if (nthr < nthr_prev) fprintf( stderr, "limit nthr=%i (few bytes)\n", nthr );

    count = count_displ(nbytes, nthr);
    int64_t* displ = count+nthr;

    // find closest (to left) newline and update corresponding displacement
    #pragma omp parallel for num_threads(nthr)
    for (int t=0; t<nthr; ++t) {
        while (displ[t] > 0) {
            if (bytes[--displ[t]] == '\n') {
                displ[t]++;
                break;
            }
        }
    }

    // update counts
    for (int t=0; t<nthr; ++t) count[t] = (t==(nthr-1) ? nbytes : displ[t+1]) - displ[t];

    results = calloc( nthr, sizeof*results );
    int64_t nrow_found = 0;
    // get the data on each thread
    #pragma omp parallel num_threads(nthr)
    {
        #ifdef USE_DOUBLE_CONVERSION
        void* dchandle = dcwrap_init();
        #endif
        int t = omp_get_thread_num();
        char* buf = bytes + displ[t];
        int64_t offset = 0;
        column_t col = {.buffer_continues=1};
        char num[64] = {0};
        VECTOR_RESERVE( results[t].v, nbytes/nthr/8 );

        int64_t my_ncol = 0, my_nrow = 0, my_ncol_min = INT64_MAX, my_ncol_max = INT64_MIN;
        while (col.buffer_continues) {
            col = get_column(buf, offset, count[t], num);
            if (col.has_number) {
                if (col.line_continues) {
                    my_ncol++;
                } else {
                    my_ncol_max = MAX(my_ncol_max,my_ncol);
                    my_ncol_min = MIN(my_ncol_min,my_ncol);
                    my_ncol = 0;
                    my_nrow++;
                }
                #ifdef USE_DOUBLE_CONVERSION
                double dnum = dcwrap_run( dchandle, num, sizeof num );
                #else
                double dnum = atof(num);
                #endif
                VECTOR_PUSH_BACK( results[t].v, dnum );
            } else if (my_ncol != 0) {
                my_nrow++;
                my_ncol = 0;
            }
            offset = col.offset;
        }
        if (t != 0) VECTOR_SHRINK_TO_FIT( results[t].v );

        #ifdef USE_DOUBLE_CONVERSION
        dcwrap_free(dchandle);
        #endif

        my_ncol = my_ncol_max + 1;
        if (my_ncol_min != my_ncol_max) {
            VECTOR_RESIZE( results[t].v, 0 );
            if (my_nrow != 0) fprintf( stderr, "error (thr#%i): #cols=%li..%li. skipping %li rows.\n",
                                       t, my_ncol_min, my_ncol_max, my_nrow );
            my_nrow = 0;
        }

        // here the output begins
        #pragma omp atomic
        nrow_found += my_nrow;

        #pragma omp barrier
        {}
        #pragma omp single
        {
            VECTOR_COPY( result, results[0].v );
            VECTOR_RESERVE( result, nrow_found*my_ncol );
            for (int o=1; o<nthr; ++o) {
                memcpy( result + result_sz, results[o].v, sizeof(double) * results[o].v_sz );
                result_sz += results[o].v_sz;
                VECTOR_FREE( results[o].v );
            }
        }
    }

    VECTOR_SHRINK_TO_FIT( result );
    *nrow = nrow_found;
    *ncol = result_sz / nrow_found;
mclean:
    if (bytes) munmap(bytes, nbytes);
    free(count);
    free(results);
    return result; // should be NULL on error
}

void savetxt_buffered( const char* fname, const double* data, int64_t nrow,
                       int64_t ncol, const char* header, int nthr ) {
    if (nthr <= 0) nthr = omp_get_max_threads();

    FILE* f = fopen(fname, "wb");
    if (!f) return;

    int64_t* count = count_displ(nrow, nthr);
    int64_t* displ = count + nthr;

    int64_t bufsz = 20 * nrow * ncol;
    char* buf = malloc( bufsz+1 );
    if (!buf) { free(count); fclose(f); return; }

    #pragma omp parallel for num_threads(nthr)
    for (int t=0; t<nthr; ++t) {
        for (int64_t i=0; i<count[t]; ++i) {
            int64_t r = displ[t]+i;
            char* colbuf = buf + 20*r*ncol;
            for (int64_t c=0; c<ncol; ++c) {
                char tmp[32] = {0};
                sprintf( tmp, "%19.12e ", data[r*ncol+c] );
                memcpy( colbuf, tmp, 20 );
                colbuf += 20;
            }
            colbuf[-1] = '\n';
        }
    }
    free(count);
    if (header) fprintf(f, "%s\n", header);
    fwrite(buf, 20, nrow*ncol, f);
    free(buf);
    fclose(f);
}

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
    ary = genfromtxt_buffered( "RAND.dat", &nrow, &ncol, -1 );
    free( ary );
    tock = wtime();
    printf( "buf: %.2f (%li×%li)\n", tock-tick, nrow, ncol );

    tick = wtime();
    ary = genfromtxt_mmap( "RAND.dat", &nrow, &ncol, -1 );
    free( ary );
    tock = wtime();
    printf( "map: %.2f (%li×%li)\n", tock-tick, nrow, ncol );
}
*/
