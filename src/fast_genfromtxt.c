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

static inline int64_t* count_displ( int64_t num, int64_t par ) {
    int64_t* count = calloc(2*par, sizeof*count);
    int64_t* displ = count + par;

    for (int64_t p=0; p<par; ++p) count[p] = num/par;
    for (int64_t p=0; p<num%par; ++p) count[p]++;

    for (int64_t p=1; p<par; ++p) displ[p] = displ[p-1]+count[p-1];
    return count;
}

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

typedef struct {
    int64_t offset;
    int line_continues;
    int buffer_continues;
    int has_number;
} column_t;

static uint64_t space_bits[4] = {4294983168,0,0,0};
static inline uint64_t isspace_custom( unsigned char chr ) {
    return space_bits[chr/64] & (1ULL<<(chr%64));
}

void fast_genfromtxt_register_space_char( unsigned char chr ) {
    space_bits[chr/64] |= (1ULL<<(chr%64));
}

void fast_genfromtxt_unregister_space_char( unsigned char chr ) {
    space_bits[chr/64] &= ~(1ULL<<(chr%64));
}

void fast_genfromtxt_reset_space_chars( void ) {
    uint64_t default_space_bits[4] = {4294983168,0,0,0};
    memcpy( space_bits, default_space_bits, sizeof(default_space_bits) );
}

#ifndef NNUMBER_MAX
#define NNUMBER_MAX 64
#endif

static inline column_t get_column( char* restrict buffer, int64_t offset, int64_t size, char* restrict number ) {
    column_t result = {.offset=offset, .line_continues=1, .buffer_continues=1, .has_number=0};

    while (isspace_custom(buffer[result.offset]) && result.offset < size)
        result.offset++;

    int nnumber = 0;
    int iscomment = 0;
    while (!isspace_custom(buffer[result.offset]) && result.offset < size) {
        char tmp = buffer[result.offset++];
        number[nnumber++] = tmp;
        if (nnumber >= NNUMBER_MAX)
            break;
        if (tmp == '#') {
            iscomment = 1;
            nnumber--;
            break;
        }
    }
    result.has_number = (nnumber > 0);
    if (nnumber==NNUMBER_MAX)
        number[NNUMBER_MAX-1] = '\0';
    else
        number[nnumber++] = '\0';

    if (iscomment) {
        while (buffer[result.offset] != '\n' && result.offset < size)
            result.offset++;
        result.line_continues = 0;
    } else {
        while (isspace_custom(buffer[result.offset]) && result.offset < size) {
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

double* fast_genfromtxt_mmap( const char* fname, int64_t* nrow, int64_t* ncol, int nthr ) {
    if (nthr <= 0) nthr = omp_get_max_threads();

    // have this here to return it on error
    *nrow = *ncol = -1;

    int fd = open(fname, O_RDONLY);
    char* bytes = NULL;
    int64_t nbytes = 0;
    int64_t* count = NULL;
    double** results = NULL;
    double* result = NULL;
    int64_t result_sz = 0;

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
    nthr = MAX(nthr, 1);
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
        char num[NNUMBER_MAX] = {0};
        ptrvec_reserve( results[t], nbytes/nthr/8 );

        int64_t my_ncol = 0, my_nrow = 0, my_ncol_min = INT64_MAX, my_ncol_max = INT64_MIN;
        while (col.buffer_continues) {
            col = get_column(buf, offset, count[t], num);
            // TODO debugging printf
            // printf( "t%i: col'%s' -> (lc%i bc%i hn%i)\n", t, num, col.line_continues, col.buffer_continues, col.has_number );
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
                ptrvec_push( results[t], dnum );
            } else if (my_ncol != 0) {
                my_nrow++;
                my_ncol--;
                my_ncol_max = MAX(my_ncol_max,my_ncol);
                my_ncol_min = MIN(my_ncol_min,my_ncol);
                my_ncol = 0;
            }
            offset = col.offset;
        }
        if (t != 0) ptrvec_shrink( results[t] );

        #ifdef USE_DOUBLE_CONVERSION
        dcwrap_free(dchandle);
        #endif

        my_ncol = my_ncol_max + 1;
        if (my_ncol_min != my_ncol_max) {
            ptrvec_resize( results[t], 0 );
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
            result = malloc(nrow_found*my_ncol* sizeof*result);
            for (int o=0; o<nthr; ++o) {
                memcpy(result + result_sz, results[o], sizeof(double)*ptrvec_sz(results[o]));
                result_sz += ptrvec_sz(results[o]);
                ptrvec_free(results[o]);
            }
        }
    }

    *nrow = nrow_found;
    if (nrow_found == 0)
        *ncol = 0;
    else
        *ncol = result_sz / nrow_found;
mclean:
    if (bytes != MAP_FAILED && bytes) munmap(bytes, nbytes);
    free(count);
    free(results);
    return result; // should be NULL on error
}

double* fast_genfromtxt_mmap_serial( const char* fname, int64_t* nrow, int64_t* ncol ) {
    // have this here to return it on error
    *nrow = *ncol = -1;

    int fd = open(fname, O_RDONLY);
    char* bytes = NULL;
    int64_t nbytes = 0;
    double* result = NULL;
    double* tmp = NULL;

    if (fd <= 0) goto mclean;

    struct stat finfo = {0};
    fstat(fd, &finfo);
    nbytes = finfo.st_size;
    if ((bytes = mmap(NULL, nbytes, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        close(fd);
        goto mclean;
    }
    close(fd);

    #ifdef USE_DOUBLE_CONVERSION
    void* dchandle = dcwrap_init();
    #endif
    int64_t offset = 0;
    column_t col = {.buffer_continues=1};
    char num[NNUMBER_MAX] = {0};
    ptrvec_reserve(tmp, nbytes/8);

    int64_t my_ncol = 0, my_nrow = 0, my_ncol_min = INT64_MAX, my_ncol_max = INT64_MIN;
    while (col.buffer_continues) {
        col = get_column(bytes, offset, nbytes, num);
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
            ptrvec_push(tmp, dnum);
        } else if (my_ncol != 0) {
            my_nrow++;
            my_ncol--;
            my_ncol_max = MAX(my_ncol_max,my_ncol);
            my_ncol_min = MIN(my_ncol_min,my_ncol);
            my_ncol = 0;
        }
        offset = col.offset;
    }
    ptrvec_shrink(tmp);

    #ifdef USE_DOUBLE_CONVERSION
    dcwrap_free(dchandle);
    #endif

    my_ncol = my_ncol_max + 1;
    if (my_ncol_min != my_ncol_max) {
        if (my_nrow != 0) fprintf(stderr, "error: #cols=%li..%li. skipping %li rows.\n",
                                  my_ncol_min, my_ncol_max, my_nrow);
        my_nrow = 0;
    }
    *nrow = my_nrow;
    if (my_nrow == 0)
        *ncol = 0;
    else
        *ncol = ptrvec_sz(tmp)/my_nrow;

    result = malloc((*nrow) * (*ncol) * sizeof*result);
    memcpy(result, tmp, (*nrow) * (*ncol) * sizeof*result);
    ptrvec_free(tmp);
mclean:
    if (bytes != MAP_FAILED && bytes) munmap(bytes, nbytes);
    return result; // should be NULL on error
}

void fast_genfromtxt_free( double* fg_buf ) { free(fg_buf); }

void fast_savetxt_buffered( const char* fname, const double* data, int64_t nrow,
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

void fast_savetxt_serial( const char* fname, const double* data, int64_t nrow,
        int64_t ncol, const char* header ) {
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

#ifndef SKIP_MAIN_COMPILATION
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

    /*
    int64_t nrow_write = 10000,
            ncol_write = 4000;
    FILE* rf = fopen( "RAND.dat", "w");
    fprintf( rf, "# header\n" );
    for (int i=0; i<nrow_write; ++i) {
        for (int j=0; j<ncol_write; ++j) {
            char endchar = ' ';
            if (j == ncol_write-1 && i != nrow_write-1)
                endchar = '\n';
            fprintf( rf, "%.5e%c", (double)rand()/(double)RAND_MAX, endchar );
            if (j == ncol_write-1 && i == nrow_write-1)
                fprintf( rf, " # comment\n" );
        }
    }
    fclose( rf );
    */

    tick = wtime();
    ary = fast_genfromtxt_mmap_serial( "RAND.dat", &nrow, &ncol );
    free( ary );
    tock = wtime();
    printf( "ser: %.2f (%li×%li)\n", tock-tick, nrow, ncol );

    tick = wtime();
    ary = fast_genfromtxt_mmap( "RAND.dat", &nrow, &ncol, -1 );
    free( ary );
    tock = wtime();
    printf( "map: %.2f (%li×%li)\n", tock-tick, nrow, ncol );
}
#endif // SKIP_MAIN_COMPILATION
