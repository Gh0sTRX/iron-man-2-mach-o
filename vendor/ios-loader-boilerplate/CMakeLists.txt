cmake_minimum_required(VERSION 3.21)
project(im2_ios_loader_objects LANGUAGES C CXX ASM)

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

option(IM2_VITAGL_ENV_PROBE "Use the asset-free native VitaGL diagnostic entry (ELF only)" OFF)
set(_entry_source source/main.c)
set(_elf_target im2_ios_loader)
if(IM2_VITAGL_ENV_PROBE)
  set(_entry_source "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/vitagl_environment_probe/main.c")
  set(_elf_target im2_vitagl_environment_probe)
endif()
add_library(im2_ios_loader_objects OBJECT
  ${_entry_source}
  source/darwin_printf.c
  source/console_printf.c
  source/macho.c
  source/macho_reloc.c
  source/macho_init.c
  source/cg_geometry.c
  source/math_bridge.c
  source/legacy_containers.cpp
  source/legacy_streams.cpp
  source/legacy_string.cpp
  source/imports.c
  source/libc_text.c
  source/exit_registry.c
  source/darwin.c
  source/fs_path.c
  source/objc.c
  source/foundation.c
  source/cfstring.c
  source/collections.c
  source/url.c
  source/url_request.c
  source/file_manager.c
  source/uikit_hierarchy.c
  source/uikit_application.c
  source/uikit_bootstrap.c
  source/uikit_loop.c
  source/time_bridge.c
  source/thread_lifecycle.c
  source/timers.c
  source/runloop.c
  source/numbers.c
  source/defaults.c
  source/cf_memory.c
  source/cfarrays.c
  source/cfbundle.c
  source/sets.c
  source/touches.c
  source/uikit_screen.c
  source/eagl_bridge.c
  source/objc_runtime.c
  source/objc_guest.c
  source/gles_arrays.c
  source/gles_float.c
  source/gles.c
  source/gles_oes.c
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

# Link the actual complete loader without creating SELF or VPK.
option(IM2_LINK_LOADER "Link full research loader ELF" OFF)
if(IM2_LINK_LOADER)
  add_executable(${_elf_target} $<TARGET_OBJECTS:im2_ios_loader_objects>)
  target_link_options(${_elf_target} PRIVATE -Wl,-q -Wl,-Map=${CMAKE_BINARY_DIR}/${_elf_target}.map)
  target_link_libraries(${_elf_target} PRIVATE
    -Wl,--whole-archive pthread -Wl,--no-whole-archive
    vitaGL vitashark SceShaccCgExt stdc++ mathneon freetype png16 z bz2 m
    kubridge_stub taihen_stub SceShaccCg_stub
    SceAppMgr_stub SceCtrl_stub SceTouch_stub SceDisplay_stub SceSysmodule_stub
    ScePower_stub SceKernelDmacMgr_stub SceGxm_stub SceCommonDialog_stub
    SceAppUtil_stub SceAudio_stub SceIofilemgr_stub SceIme_stub)
endif()
