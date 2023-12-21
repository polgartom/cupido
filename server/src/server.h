#ifndef H_CUPIDO_SERVER
#define H_CUPIDO_SERVER

#include "core.h"

#define CRLF "\r\n"
#define CRLF_LEN constexpr(strlen(CRLF))
#define HTTP_1_1 "HTTP/1.1"

#define RECV_BUF_SIZE BYTES_TO_KB(256)

const int MAX_CLIENTS = 128; 
const int REQUEST_MAX_SIZE = (1024*1024*64);

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

enum Http_Method {
    HTTP_METHOD_NONE = 0,
    
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_DELETE,
    
    HTTP_METHOD_COUNT,
};

enum Http_Response_Status {
    HTTP_OK                              = 200,
    HTTP_CREATED                         = 201,
    HTTP_ACCEPTED                        = 202,
    HTTP_NO_CONTENT                      = 204,
    HTTP_MOVED_PERMANENTLY               = 301,
    HTTP_FOUND                           = 302,
    HTTP_SEE_OTHER                       = 303,
    HTTP_NOT_MODIFIED                    = 304,
    HTTP_TEMPORARY_REDIRECT              = 307,
    HTTP_PERMANENT_REDIRECT              = 308,

    HTTP_BAD_REQUEST                     = 400,
    HTTP_UNAUTHORIZED                    = 401,
    HTTP_FORBIDDEN                       = 403,
    HTTP_NOT_FOUND                       = 404,
    HTTP_METHOD_NOT_ALLOWED              = 405,
    HTTP_CONFLICT                        = 409,
    HTTP_PAYLOAD_TOO_LARGE               = 413,
    HTTP_UNSUPPORTED_MEDIA_TYPE          = 415,
    HTTP_UNPROCESSABLE_ENTITY            = 422,
    HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,

    HTTP_INTERNAL_SERVER_ERROR           = 500,
    HTTP_NOT_IMPLEMENTED                 = 501,
    HTTP_HTTP_VERSION_NOT_SUPPORTED      = 505
};

enum Http_Request_State {
    HTTP_STATE_CONN_RECEIVED = 0,
    HTTP_STATE_HEADER_PARSED = 0b00000001,
};

struct Request {
    u32 id;

    bool connected;
    bool should_close;
    SOCKET socket = INVALID_SOCKET;

    String raw_body;

    // @Todo: Union http stuffs? 
    Http_Request_State state;
    
    Http_Method method;
    String path;
    String protocol;
    
    Mime_Type content_type;
    s32 content_length = -1;
    
    String header;
    String body;
    char buf[4096];
};

struct Server {
    SOCKET socket = INVALID_SOCKET;
    int port;
    bool running = false; 
    
    Request *clients;
    bool    free_clients[MAX_CLIENTS];
};

Http_Method http_method_str_to_enum(String method)
{
    if (method == "GET")    return HTTP_METHOD_GET;
    if (method == "POST")   return HTTP_METHOD_POST;
    if (method == "DELETE") return HTTP_METHOD_DELETE;
    
    return HTTP_METHOD_NONE;
}

Mime_Type content_type_str_to_enum(String s)
{
    bool ok = true;
    String right;
    String left = split(s, "; ", &right, &ok);
    
    #define RET_IF_MATCH(_cstr, _enum) if (left == _cstr) return _enum;
        
    if (string_starts_with_and_step(&left, "application/")) {
        RET_IF_MATCH("json",         Mime_App_Json);
        RET_IF_MATCH("octet-stream", Mime_App_OctetStream);
        RET_IF_MATCH("pdf",          Mime_App_Pdf);
        RET_IF_MATCH("gzip",         Mime_App_Gzip);
        RET_IF_MATCH("x-tar",        Mime_App_Tar);
        RET_IF_MATCH("vnd.rar",      Mime_App_Rar);
    } 
    else if (string_starts_with_and_step(&left, "text/")) {
        RET_IF_MATCH("plain", Mime_Text_Plain);
        RET_IF_MATCH("html",  Mime_Text_Html);
    }
    else if (string_starts_with_and_step(&left, "image/")) {
        RET_IF_MATCH("jpeg", Mime_Image_Jpg);
        RET_IF_MATCH("png",  Mime_Image_Png);
        RET_IF_MATCH("gif",  Mime_Image_Gif);
        RET_IF_MATCH("webp",  Mime_Image_Webp);
    }
    
    #undef RET_IF_MATCH
    
    return Mime_None;
}

#endif 