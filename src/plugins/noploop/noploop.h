/*
 * NOPLOOP benchmark test. Method adopted from Brendan Greg's NOP loop benchmark example.  
 */

#ifndef _noploop_h
#define _noploop_h

#define _GNU_SOURCE

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Need to make this volatile so the compiler does no optimize the 0x90 out
#define NOP0 __asm__ __volatile__ ("nop" : : : "eax");
#define NOP1 NOP0 NOP0
#define NOP2 NOP1 NOP1
#define NOP3 NOP2 NOP2
#define NOP4 NOP3 NOP3
#define NOP5 NOP4 NOP4
#define NOP6 NOP5 NOP5
#define NOP7 NOP6 NOP6
#define NOP8 NOP7 NOP7
#define NOP9 NOP8 NOP8
#define NOP10 NOP9 NOP9
#define NOP11 NOP10 NOP10
#define NOP12 NOP11 NOP11

#define NOPS 4096
#define NLOOP 10000000

#ifdef __LP64__
#define FMT_U64 "ld"
#else
#define FMT_U64 "lld"
#endif

typedef struct noploopctx noploopctx;

struct noploopctx {
    unsigned int loops;
};

#endif