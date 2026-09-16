/*
 ============================================================================
 Name        : hev-task-execute.S
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description :
 ============================================================================
 */

#if defined(__arm__)

# include "arch/arm/hev-task-execute-arm.s"

#elif defined(__aarch64__)

# include "arch/arm/hev-task-execute-aarch64.s"

#else

# error "Unsupported platform!"

#endif

#if defined(__ELF__) && defined(__linux__)
/* An executable stack is *not* required for these functions. */
    .section .note.GNU-stack,"",%progbits
    .previous
#endif
