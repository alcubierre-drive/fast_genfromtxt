#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void* dcwrap_init( void );
void dcwrap_free( void* handle );
double dcwrap_run( const void* handle, const char* buf, long buf_sz );

#ifdef __cplusplus
}
#endif
