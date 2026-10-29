cmake_minimum_required(VERSION 3.21)
project(im2_ios_loader_objects LANGUAGES C ASM)

# Compile-only target: no ELF/SELF/VPK is produced.
# Configure with: cmake -S vendor/ios-loader-boilerplate -B build/loader-objects
#   -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
if(NOT DEFINED ENV{VITASDK})
  message(FATAL_ERROR "VITASDK environment variable is required")
endif()
if(NOT CMAKE_C_COMPILER MATCHES "arm-vita-eabi")
  message(FATAL_ERROR "VitaSDK cross-compiler required; configure with -DCMAKE_TOOLCHAIN_FILE")
endif()

set(_sdk "$ENV{VITASDK}/arm-vita-eabi")
# This stage intentionally checks only compilation prerequisites.
# Link-time dependencies are verified separately by tools/check_vita_dependencies.sh.
foreach(_header "kubridge.h" "vitaGL.h")
  if(NOT EXISTS "${_sdk}/include/${_header}")
    message(FATAL_ERROR "Missing required header: ${_header}")
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
target_compile_options(im2_ios_loader_objects PRIVATE -O2 -g -ffixed-r9)
message(STATUS "Loader object compilation only. Link, RX validation and VPK are separate gates.")
