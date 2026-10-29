cmake_minimum_required(VERSION 3.21)
if(NOT DEFINED ENV{VITASDK})
  message(FATAL_ERROR "Use pinned VitaSDK 2026.08 (VITASDK environment variable)")
endif()
include("$ENV{VITASDK}/share/vita.toolchain.cmake" REQUIRED)
project(im2_ios_loader_objects C ASM)

# Compile-only readiness target. NO eboot.bin, SELF or VPK is produced.
# Includes all vendored translation units; still no linking or VPK packaging.
set(_sdk "$ENV{VITASDK}/arm-vita-eabi")
foreach(_header "kubridge.h" "vitaGL.h")
  if(NOT EXISTS "${_sdk}/include/${_header}")
    message(FATAL_ERROR "Missing VitaSDK header: ${_header}")
  endif()
endforeach()
foreach(_lib "kubridge_stub" "vitaGL" "vitashark" "SceShaccCgExt")
  if(NOT EXISTS "${_sdk}/lib/lib${_lib}.a")
    message(FATAL_ERROR "Missing VitaSDK library: ${_lib} (install with vdpm)")
  endif()
endforeach()

add_library(im2_ios_loader_objects OBJECT
  source/main.c
  source/darwin_printf.c
  source/macho.c
  source/imports.c
  source/darwin.c
  source/objc.c
  source/gles.c
  source/coreaudio.c
  source/input.c
  source/patch.c
  source/cgbi.c
  source/log.c
  source/vitagl_no_splash.c
  source/asm.S
)
target_include_directories(im2_ios_loader_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/source"
  "${_sdk}/include/freetype2"
)
target_compile_options(im2_ios_loader_objects PRIVATE
  -O2 -g -ffixed-r9 -mfloat-abi=softfp
)
message(STATUS "Compile-only target; runtime imports and RX permissions still need verification.")
