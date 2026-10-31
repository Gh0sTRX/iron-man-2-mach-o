/* SPDX-License-Identifier: MIT. Reuse SDK C text routines with explicit ABI checks.
 * Locale-dependent calls follow the native C-library locale; no locale emulation.
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include <stdint.h>
#include "imports.h"
_Static_assert(sizeof(long)==4 && sizeof(size_t)==4 && sizeof(wchar_t)==4,
               "Original ARM C text ABI requires 32-bit long/size_t/wchar_t");
SOFTFP static double text_u64_double(uint64_t value){return (double)value;}
const import_entry libc_text_imports[]={
 F(atol,atol),F(strcasecmp,strcasecmp),F(strncasecmp,strncasecmp),F(strlcpy,strlcpy),F(strcoll,strcoll),
 F(wcscmp,wcscmp),F(wcscpy,wcscpy),F(wcslen,wcslen),F(wcstombs,wcstombs),F(__floatundidf,text_u64_double),
};
const int libc_text_imports_count=sizeof(libc_text_imports)/sizeof(libc_text_imports[0]);
