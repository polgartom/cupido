#ifndef H_CUPIDO_CORE
#define H_CUPIDO_CORE

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <tchar.h>
#include <winsock2.h>

#include <iostream>

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
// #define STRUCT_COPY(dest, src) (memcpy(&dest, &src, sizeof(dest)))

#include "new_string.h"
#include "array.h"

#define CRLF "\r\n"
#define CRLF_LEN constexpr(strlen(CRLF))
#define HTTP_1_1 "HTTP/1.1"

// String read_entire_file(char *filename)
// {
//     FILE *fp = fopen(filename, "rb");
//     ASSERT(fp, "Failed to open file! Filename: %s\n", filename);
    
//     fseek(fp, 0, SEEK_END);
//     u32 fsize = ftell(fp);
//     rewind(fp);
    
//     String s = string_make_alloc(fsize+1); // +1, because we'll insert a line break to deal with the EOF easier
//     s.data[s.count-1] = '\n';
    
//     fread(s.data, fsize, 1, fp);
//     fclose(fp);
    
//     return s;
// }

enum Mime_Type {
    Mime_None = 0,
    Mime_OctetStream = 1
};

struct Request {
    SOCKET socket;
    char buf[4096];
    int buf_len;
    u32 flags = 0;
    
    Mime_Type content_type;
    s32 content_length = -1;
};

#define HEADER_PARSED 0b0001
#define BODY_PARSED   0b0010

#endif 