#ifndef H_CUPIDO_CORE
#define H_CUPIDO_CORE

#define __STDC_WANT_LIB_EXT1__ 1

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include <iostream>

#include "defer.h"

typedef char s8;
typedef short s16;
typedef int s32;
typedef long s64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

#define COLOR_DEFAULT "\033[0m"
#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_PURPLE "\033[0;35m"
#define COLOR_CYAN "\033[0;36m"

#define ASSERT(__cond, __fmt_msg, ...) { \
    if (!(__cond)) { \
        printf(COLOR_RED __fmt_msg COLOR_DEFAULT "\n", ##__VA_ARGS__); \
        assert(__cond); \
    } \
}

#define ZERO_MEMORY(dest, len) (memset(((u8 *)dest), 0, (len)))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr)[0])
#define CSTR_LEN(x) (x != NULL ? strlen(x) : 0)
#define XSTR(x) #x

#define BYTES_TO_KB(_bytes) ((_bytes) * 1024)
#define BYTES_TO_MB(_bytes) (BYTES_TO_KB(_bytes) * 1024)
#define BYTES_TO_GB(_bytes) (BYTES_TO_MB(_bytes) * 1024)

#define print printf

#include "new_string.h"

String read_entire_file(String fname, const char *mode)
{
    STRING_TO_CSTR_ALLOCA(fname, fname_cstr);
    FILE *fp = fopen(fname_cstr, mode);
    
    ASSERT(fp, "Failed to open the file " SFMT " ; errno: %d\n", SARG(fname), errno); // @Temporary
    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    String s = string_create(fsize);
    s.count = fsize;
    ASSERT(fread(s.data, 1, fsize, fp) == fsize, "");
    
    fclose(fp);
    
    return s;
}

#endif 