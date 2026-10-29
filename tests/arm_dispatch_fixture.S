/* Own synthetic ARM functions for executing the production assembly in Unicorn. */
.syntax unified
.arm
.text
.global ml_unimpl_log
ml_unimpl_log: bx lr
.global im2_objc_lookup_imp
im2_objc_lookup_imp:
    cmp r2, #0
    ldreq r0, =test_word_imp
    ldrne r0, =test_struct_imp
    bx lr
.global test_word_imp
.type test_word_imp,%function
test_word_imp:
    add r0, r2, r3
    ldr ip, [sp]
    add r0, r0, ip
    bx lr
.global test_struct_imp
.type test_struct_imp,%function
test_struct_imp:
    str r3, [r0]
    ldr ip, [sp]
    str ip, [r0, #4]
    bx lr
.global test_arm_guest
.type test_arm_guest,%function
test_arm_guest:
    add r0, r0, r1
    add r0, r0, r2
    add r0, r0, r3
    mov r9, #99
    bx lr
.global test_arm_guest6
.type test_arm_guest6,%function
test_arm_guest6:
    add r0, r0, r1
    add r0, r0, r2
    add r0, r0, r3
    ldr ip, [sp]
    add r0, r0, ip
    ldr ip, [sp, #4]
    add r0, r0, ip
    mov r9, #99
    bx lr
.thumb
.thumb_func
.global test_thumb_guest
.type test_thumb_guest,%function
test_thumb_guest:
    adds r0, r0, r1
    bx lr
