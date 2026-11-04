/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only replacement for the bounded retail varargs formatting bridge. */
#include <stdarg.h>
#include "openrac/guest.h"

int openrac_lib_sly1_vformat(gaddr destination, int capacity, gaddr format,
                             va_list arguments);

int func_001E20B8(gaddr destination, int capacity, gaddr format, ...)
{
    va_list arguments;
    int result;
    if (capacity <= 0) return -1;
    va_start(arguments, format);
    result = openrac_lib_sly1_vformat(destination, capacity, format, arguments);
    va_end(arguments);
    return result;
}
