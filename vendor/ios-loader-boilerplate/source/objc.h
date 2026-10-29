/**
 * @file objc.h
 * @brief Objective-C selector dispatch runtime and on-screen keyboard IME bridge.
 */
#pragma once
#include <stdint.h>

void objc_init(void);
void ime_poll(void);
int ime_active(void);

#include "imports.h"
const import_entry *objc_class_import(const char *name);
