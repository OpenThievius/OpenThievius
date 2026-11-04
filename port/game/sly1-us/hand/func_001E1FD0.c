/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only replacement for the retail varargs-to-va_list bridge. */
#include <stdarg.h>
#include "openrac/guest.h"

int openrac_lib_sly1_vformat(gaddr destination, int capacity, gaddr format,
                             va_list arguments);

int func_001E1FD0(gaddr destination, gaddr format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = openrac_lib_sly1_vformat(destination, 0, format, arguments);
    va_end(arguments);
    return result;
}
