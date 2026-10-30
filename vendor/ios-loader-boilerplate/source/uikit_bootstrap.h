/* SPDX-License-Identifier: MIT
 * Staged legacy UIApplicationMain launch path. This invokes the owner's actual
 * delegate launch method, but refuses to claim a UIKit run loop exists.
 */
#pragma once
#include <stdint.h>

typedef uint32_t (*Im2UIKitGuestCall)(void *imp,uint32_t self,
    uint32_t selector,uint32_t arg0,uint32_t arg1);

enum {
    IM2_UIKIT_BOOT_INVALID = -1,
    IM2_UIKIT_BOOT_NEEDS_RUN_LOOP = -2
};

/* Delegate class is resolved from a registered NSString/CFString, not guessed.
 * Caller must have registered guest classes and mapped executable memory.
 * On successfully dispatching the legacy didFinishLaunching callback, returns
 * NEEDS_RUN_LOOP (never success), pending Vita event/render loop support. */
int im2_uikit_launch_once(int argc,char *const *argv,
    void *principal_class_name,void *delegate_class_name,
    Im2UIKitGuestCall guest_bridge);

/* Drains launch temporaries before entering per-turn pools, keeping delegate alive. */
int im2_uikit_launch_finish_pool(void);

/* Owner of the initialized guest delegate; teardown for tests/exit. */
int im2_uikit_launch_release_delegate(void);
