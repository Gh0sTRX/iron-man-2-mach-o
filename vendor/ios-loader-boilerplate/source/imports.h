/**
 * @file imports.h
 * @brief Dynamic import table definitions and SoftFP calling conventions.
 */
#pragma once
#include <stdint.h>

/**
 * @brief Represents a single imported symbol binding.
 */
enum { IM2_IMPORT_FUNCTION=0, IM2_IMPORT_DATA=1, IM2_IMPORT_ABI_ENTRY=2 };

typedef struct {
    const char *name; /**< Mach-O symbol name (including leading '_' if present). */
    uintptr_t addr;   /**< Destination function address or data pointer. */
    int data;         /**< 0 = bridged function, 1 = data, 2 = assembly entry that owns its ABI boundary. */
} import_entry;

/**
 * @brief Resolves an imported symbol by Mach-O name across all registered tables.
 * @param[in] name Symbol name string to locate.
 * @return Pointer to matching import_entry, or NULL if unhandled.
 */
const import_entry *imports_lookup(const char *name);

extern const import_entry cg_geometry_imports[];
extern const int cg_geometry_imports_count;

extern const import_entry math_bridge_imports[];
extern const int math_bridge_imports_count;

extern const import_entry legacy_container_imports[];
extern const int legacy_container_imports_count;

extern const import_entry legacy_stream_imports[];
extern const int legacy_stream_imports_count;

extern const import_entry legacy_string_imports[];
extern const int legacy_string_imports_count;

// Modular subsystem symbol tables
extern const import_entry libc_text_imports[];
extern const int libc_text_imports_count;

extern const import_entry time_bridge_imports[];
extern const int time_bridge_imports_count;

extern const import_entry exit_registry_imports[];
extern const int exit_registry_imports_count;

extern const import_entry darwin_imports[];
extern const int darwin_imports_count;

extern const import_entry gles_float_imports[];
extern const int gles_float_imports_count;

extern const import_entry gles_imports[];
extern const int gles_imports_count;

extern const import_entry coreaudio_imports[];
extern const int coreaudio_imports_count;

extern const import_entry objc_imports[];
extern const int objc_imports_count;

extern const import_entry eagl_imports[];
extern const int eagl_imports_count;

extern const import_entry patch_imports[];
extern const int patch_imports_count;

extern const import_entry cf_memory_imports[];
extern const int cf_memory_imports_count;

extern const import_entry cfarray_imports[];
extern const int cfarray_imports_count;

extern const import_entry cfbundle_imports[];
extern const int cfbundle_imports_count;

// iOS ARM SoftFP calling convention: float and double arguments pass in integer registers (r0..r3).
// Because VitaSDK targets hardfp by default, any imported function that passes or returns floats
// must be decorated with this attribute.
#define SOFTFP __attribute__((pcs("aapcs")))

#define F(name, fn)    {"_" #name, (uintptr_t)(fn), 0}
#define F_RAW(name, fn){name, (uintptr_t)(fn), 0}
#define D(name, addr)  {"_" #name, (uintptr_t)(addr), 1}

#define F_ABI(name, fn) {"_" #name, (uintptr_t)(fn), IM2_IMPORT_ABI_ENTRY}
