#include "double_conversion_wrap.h"
#include <double-conversion/string-to-double.h>

using double_conversion::StringToDoubleConverter;

void* dcwrap_init( void ) {
    StringToDoubleConverter* SDC = new StringToDoubleConverter(
            StringToDoubleConverter::ALLOW_TRAILING_JUNK|
            StringToDoubleConverter::ALLOW_LEADING_SPACES, 0.0, 0.0, "INF",
            "NAN" );
    return static_cast<void*>(SDC);
}

void dcwrap_free( void* handle ) {
    StringToDoubleConverter* SDC = static_cast<StringToDoubleConverter*>(handle);
    delete SDC;
}

double dcwrap_run( const void* handle, const char* buf, long buf_sz ) {
    const StringToDoubleConverter* SDC = static_cast<const StringToDoubleConverter*>(handle);
    int nc = -1;
    return SDC->StringToDouble( buf, buf_sz, &nc );
}

