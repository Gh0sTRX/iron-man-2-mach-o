/* Diagnostic-only router over the unmodified production GLES table.
 * No Mach-O, Objective-C, filesystem or game import subsystems are linked. */
#include <string.h>
#include "imports.h"
const import_entry *imports_lookup(const char *name) {
    for (int i=0;i<gles_imports_count;i++)
        if (!strcmp(name,gles_imports[i].name)) return &gles_imports[i];
    return NULL;
}
