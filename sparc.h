/*
 * sparc.h
 * Ryan Mallon (2006)
 *
 * Functions/constants for the Sparc backend
 *
 */
#ifndef _SPARC_H_
#define _SPARC_H_

#define WORD_SIZE 4

#define SPARC_OUTS 8
#define SPARC_LOCALS 16
#define SPARC_INS 24

#define STACK_HEADER_SIZE 112
#define STACK_ALIGN 8
#define STACK_LOCAL_BASE 96
#define STACK_ARGS_BASE 92

/* GCC does it this way, so we follow that */
#define STACK_FP_BASE 20

#define LOCAL_PREFIX "%l"
#define SP_PREFIX "%sp"
#define FP_PREFIX "%fp"
#define SCRATCH_REG "%g1"

/* Maximum number of args that can be passed in registers */
#define REG_ARGS 6

#define MAX_CONST 4095
#define CONST_BITS 12
#define LOW_BIT_MASK  0x00000fff
#define HIGH_BIT_MASK 0xfffff000

int fp_offset(int, int, int);
int sp_offset(int, int);
int align(int);

#endif /* _SPARC_H_ */
