#ifndef SWISS_H
#define SWISS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

typedef unsigned char   u8;
typedef char            i8;
typedef unsigned short  u16;
typedef short           i16;
typedef unsigned int    u32;
typedef int             i32;
typedef unsigned long   u64;
typedef long            i64;
typedef float           f32;
typedef double          f64;
typedef const char*     lstring;
typedef char*           string;

void swiss_log(lstring prefix, lstring fmsg, va_list ap);
void swiss_log_error(lstring fmsg, ...);
void swiss_log_info(lstring fmsg, ...);

#ifdef SWISS_IMPL

void swiss_log(lstring prefix, lstring fmsg, va_list ap) {
    printf("%s: ", prefix);
    vprintf(fmsg, ap);
    printf("\n");
}

void swiss_log_error(lstring fmsg, ...) {
    va_list args;
    va_start(args, fmsg);
    swiss_log("ERROR", fmsg, args);
    va_end(args);
}

void swiss_log_info(lstring fmsg, ...) {
    va_list args;
    va_start(args, fmsg);
    swiss_log("INFO", fmsg, args);
    va_end(args);
}

#endif /* SWISS_IMPL */

#endif /* SWISS_H */
