cmake_minimum_required(VERSION 3.21)
project(im2_device_diagnostics C CXX ASM)
# Deliberately separate from the normal loader/CI configure and release gates.
if(NOT DEFINED ENV{VITASDK} OR NOT CMAKE_C_COMPILER MATCHES "arm-vita-eabi")
  message(FATAL_ERROR "Configure with the VitaSDK toolchain")
endif()
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
set(IM2_VITAGL_ENV_PROBE ON CACHE BOOL "Diagnostic only" FORCE)
set(IM2_LINK_LOADER ON CACHE BOOL "Diagnostic only" FORCE)
add_subdirectory(../../vendor/ios-loader-boilerplate graphics)
set_target_properties(im2_vitagl_environment_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
vita_create_self(vitagl-diagnostic.self im2_vitagl_environment_probe)
vita_create_vpk(im2-vitagl-diagnostic.vpk IM2G00001 vitagl-diagnostic.self
  VERSION 01.00 NAME "IM2 VitaGL Diagnostic")

add_executable(im2_vm_diagnostic ../vita_exec_probe/main.c)
target_compile_options(im2_vm_diagnostic PRIVATE -Wall -Wextra -Werror -marm)
target_link_libraries(im2_vm_diagnostic SceIofilemgr_stub)
vita_create_self(vm-diagnostic.self im2_vm_diagnostic)
vita_create_vpk(im2-vm-diagnostic.vpk IM2V00001 vm-diagnostic.self
  VERSION 01.00 NAME "IM2 VM Cache Diagnostic")
