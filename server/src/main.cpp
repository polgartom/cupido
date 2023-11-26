#include "core.h"

#define print printf
 
SOCKET create_listening_socket(int port) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return -1;
    }
 
    // Create a socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket. Error code: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }
 
    // Bind the socket to a specific port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);
    if (bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket. Error code: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return -1;
    }
 
    // Start listening for incoming connections
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen for connections. Error code: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return -1;
    }
    
    u64 blocking_is_enabled = 1;
    int r = ioctlsocket(listenSocket, FIONBIO, &blocking_is_enabled);
    if (r != NO_ERROR) {
        printf("ioctlsocket failed with error: %ld\n", r);
        return -1;
    }
 
    return listenSocket;
}

#define EXIT() \
    closesocket(clientSocket); \
    closesocket(listenSocket); \
    WSACleanup(); \
    return 1;

inline bool http_header_parse_line(String line, String *key, String *value)
{
    bool success = true;
    *key = split(line, ": ", value, &success);
    return success;
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
    
    #undef RET
    
    return Mime_None;
}

int main() {
    int port = 6969;
 
    // Create a listening socket
    SOCKET listenSocket = create_listening_socket(port);
    if (listenSocket == -1) {
        std::cerr << "Failed to create listening socket." << std::endl;
        return 1;
    }
 
    std::cout << "Listening socket created. Socket descriptor: " << listenSocket << std::endl;

    fd_set list = {0};
    ZERO_MEMORY(&list, sizeof(fd_set));
 
    fd_set sl = {0};
    FD_ZERO(&sl);
    FD_SET(listenSocket, &sl);

    TIMEVAL tt = {0};
    tt.tv_sec = 60;
 
    while (true) {

        // Wait for incoming connections
        int sr = select(1, &sl, NULL, NULL, &tt);
        if (sr == SOCKET_ERROR) {
            std::cerr << "select() is failed. Error code: " << WSAGetLastError() << std::endl;
            return 1;
        } else if (sr == 0) {
            std::cerr << "zero!!! the time limit expired!! WHAT?" << std::endl;
            return 1;
        }
        
        // Accept a new connection
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to accept connection. Error code: " << WSAGetLastError() << std::endl;
            EXIT();
        }

        FD_ZERO(&list);
        FD_SET(clientSocket, &list);
 
        Request req = {0};
        ZERO_MEMORY(&req, sizeof(Request));
        req.socket = clientSocket;
        
        char tmp_buf[4096] = {0};

        while (true) {
            int err;
            ZERO_MEMORY(tmp_buf, sizeof(tmp_buf));
            
            // int sr = select(1, &list, NULL, NULL, &tt);
            // err = WSAGetLastError();
            int r = recv(req.socket, tmp_buf, 4096-1, 0);
            err = WSAGetLastError();
            // print("> recv: %d ; err: %d\n", r, err);
            
            if (r == 0) {
                fprintf(stderr, "Connection is closed!");
                return false;
            } else if (err == WSAEWOULDBLOCK) {
                // print("WSAEWOULDBLOCK\n");
                continue;            
            } else if (r == SOCKET_ERROR) {
                fprintf(stderr, "SOCKET ERROR - Connection is closed!");
                std::cerr << "SOCKET ERROR. Error code: " <<  WSAGetLastError() << std::endl;
                return false;
            }

            join(&req.buf, tmp_buf);
            
            if (!(req.state & HTTP_STATE_HEADER_PARSED)) {
                // Locate end of the http header    
                int end = find_index_from_left(req.buf, CRLF CRLF);
                if (end != -1) {
                    req.raw_header = chop(req.buf, end, &req.raw_body);
                    req.raw_body = advance(req.raw_body, strlen(CRLF CRLF));
                    
                    print("\n\n---HTTP_HEADER--------\n\n");
                    print(SFMT, SARG(req.raw_header));
                    print("\n\n---HTTP_HEADER_END----\n\n");

                    print("> buf: %d ; header: %d ; body: %d\n", req.buf.count, req.raw_header.count, req.raw_body.count);
                    req.state = HTTP_STATE_HEADER_PARSED;
                    
                    bool ok = true;
                    bool has_more_line = true;
                    String h = req.raw_header;
                    String line; 
                    String key; 
                    String val;
                    line = split(h, CRLF, &h, &has_more_line);
                    assert(has_more_line);
                    
                    if (string_starts_with_and_step(&line, "GET ")) {
                        req.method = Http_Method_Get;
                    } else if (string_starts_with_and_step(&line, "POST ")) {
                        req.method = Http_Method_Post;
                    } else if (string_starts_with_and_step(&line, "DELETE ")) {
                        req.method = Http_Method_Delete;
                    } else {
                        ASSERT(0, "Http method not supported! -> " SFMT "\n", SARG(line));
                    }
                    
                    {
                        req.path = split(line, " ", &line, &ok);
                        ASSERT(ok, "Invalid http header! Malformed path. -> " SFMT "\n", SARG(line)); // can be?
                        ASSERT(line == HTTP_1_1, "Invalid http header! Protocol not supported. -> " SFMT "\n", SARG(line));
                        req.protocol = line;
                    }
                    
                    while (has_more_line) {
                        line = split(h, CRLF, &h, &has_more_line);
                        ASSERT(line.count != 0, "Invalid http header!"); // can be?

                        ok = http_header_parse_line(line, &key, &val);
                        ASSERT(ok, "Failed to parse header line: " SFMT "\n", SARG(line));
                        
                        if (key == "Content-Type") {
                            bool other_half_is_set = false;
                            String other_half;
                            val = split(val, "; ", &other_half, &other_half_is_set);
                            if (other_half_is_set) {
                                // @Todo
                                // printf("Other half of Content-Type: |" SFMT "|\n", SARG(other_half));
                            }
                            
                            req.content_type = content_type_str_to_enum(val);
                            if (req.content_type == Mime_None) {
                                printf("Content-Type not supported: " SFMT " \n", SARG(val));
                            }
                            
                            // printf("Content-Type: " SFMT " (%d)\n", SARG(val), req.content_type);
                        } 
                        else if (key == "Content-Length") {
                            req.content_length = string_to_int(val);
                            ASSERT(errno == 0, "Failed to convert string to int! Line: |" SFMT "|\n", SARG(line));
                            // printf("Content-Length: %d\n", req.content_length);
                        }

                        print("> key: |" SFMT "| ; val: |" SFMT "| \n", SARG(key), SARG(val));
                    }
                    
                }
            
                print("");
            } 
            else if (req.state & HTTP_STATE_HEADER_PARSED) {
                
            }
            
            
        }   
 
        // Close the client socket
        closesocket(clientSocket);
    }
 
    // Close the listening socket
    closesocket(listenSocket);
 
    // Cleanup Winsock
    WSACleanup();
 
    return 0;
}