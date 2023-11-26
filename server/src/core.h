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

#include "new_string.h"
#include "array.h"

#define CRLF "\r\n"
#define CRLF_LEN constexpr(strlen(CRLF))
#define HTTP_1_1 "HTTP/1.1"

const int REQUEST_MAX_SIZE = (1024*1024*64);

enum Http_Method {
    Http_Method_None = 0,

    Http_Method_Get,
    Http_Method_Post,
    Http_Method_Delete,
    
    Http_Method_Count,
}; 

enum Mime_Type {
    Mime_None = 0,
        
    Mime_App_OctetStream,
    Mime_App_Zip,
    Mime_App_Gzip,
    Mime_App_Tar,
    Mime_App_Rar,
    Mime_App_Pdf,
    Mime_App_Json,
    
    Mime_Text_Plain,
    Mime_Text_Html,
    
    Mime_Image_Jpg,
    Mime_Image_Png,
    Mime_Image_Gif,
    Mime_Image_Webp,
    
    Mime_Audio_Mp3,
    Mime_Audio_Wav,
    Mime_Audio_Webm,
    
    Mime_Video_Mp4,
    Mime_Video_Webm,
    
    Mime_Count
};

enum Http_Request_State {
    HTTP_STATE_CONN_RECEIVED = 0,
    HTTP_STATE_HEADER_PARSED = 0b00000001,
};

struct Request {
    SOCKET socket;

    String buf;
    String raw_header;
    String raw_body;
    
    int buf_len;
    u32 flags = 0;

    // @Todo: Union http stuffs? 
    Http_Request_State state;
    
    Http_Method method;
    String path;
    String protocol;
    
    Mime_Type content_type;
    s32 content_length = -1;
};

#endif 