/*****************************************************************************
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

  .text
  .syntax unified
  .thumb
  .align 2
  .thumb_func

  .global Reset_Handler
  .extern main

Reset_Handler:

#if defined(__thumb__) && !defined(__thumb2__)
  ldr r2, =__stack_end__
  mov sp, r2
#else
  ldr sp, =__stack_end__
#endif

  ldr r2, =__bss_start__
  ldr r3, =__bss_end__
  ldr r4, =0
zero_bss_loop:
  cmp r2, r3
  beq zero_bss_end
#if defined(__thumb__) && !defined(__thumb2__)
  strb r4, [r2]
  adds r2, r2, #1
#else
  strb r4, [r2], #1
#endif
  b zero_bss_loop
zero_bss_end:

  bl main

  b .
