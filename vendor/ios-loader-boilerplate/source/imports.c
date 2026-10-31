/**
 * @file imports.c
 * @brief Dynamic import lookup router across all subsystem tables.
 */
#include <string.h>

#include "imports.h"
#include "objc.h"

static const struct {
    const import_entry *table;
    const int *count;
} tables[] = {
    {cg_geometry_imports, &cg_geometry_imports_count},
    {math_bridge_imports, &math_bridge_imports_count},
    {legacy_container_imports, &legacy_container_imports_count},
    {legacy_string_imports, &legacy_string_imports_count},
    {patch_imports, &patch_imports_count},
    {libc_text_imports, &libc_text_imports_count},
    {darwin_imports, &darwin_imports_count},
    {gles_float_imports, &gles_float_imports_count},
    {gles_imports, &gles_imports_count},
    {coreaudio_imports, &coreaudio_imports_count},
    {objc_imports, &objc_imports_count},
    {eagl_imports, &eagl_imports_count},
    {cf_memory_imports, &cf_memory_imports_count},
    {cfarray_imports, &cfarray_imports_count},
    {cfbundle_imports, &cfbundle_imports_count},
};

const import_entry *imports_lookup(const char *name) {
    for (unsigned t = 0; t < sizeof(tables) / sizeof(tables[0]); t++) {
        if (!tables[t].table || !tables[t].count)
            continue;
        for (int i = 0; i < *tables[t].count; i++) {
            if (!strcmp(tables[t].table[i].name, name))
                return &tables[t].table[i];
        }
    }
    return objc_class_import(name);
}
