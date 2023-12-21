#include "server.h"
 
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

bool server_create(Server *s, int port)
{
    s->socket = create_listening_socket(port);
    if (s->socket == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create listening socket.\n");
        return false;
    }
    
    s->port = port;
    s->clients = (Request *)malloc(sizeof(Request) * MAX_CLIENTS);
    for (auto i = 0; i < MAX_CLIENTS; i++) s->clients[i].id = i;
    assert(s->clients);
    memset(s->free_clients, 1, MAX_CLIENTS);
     
    return true;
}

void server_shutdown(Server *s, bool force = false)
{
    printf("[server]: Shutdown...\n");

    if (s->running) {
        ASSERT(closesocket(s->socket) == 0, "Failed to close server (listen socket) socket!\n");
        printf("[server]: Socket closed!\n");
    }
        
    if (force) {
        printf("[server]: WSACleanup()\n");
        ASSERT(WSACleanup() != 0, "Failed to run WSACleanup(). Error code: %d\n", WSAGetLastError());
    }
    
    s->running = false;
    
    printf("[server]: Stopped!\n");
}

inline bool http_header_parse_line(String line, String *key, String *value)
{
    bool success = true;
    *key = split(line, ": ", value, &success);
    return success;
}

inline void close_client(Server *s, Request *c)
{
    if (c->connected) {
        ASSERT(closesocket(c->socket) == 0, "Failed to close the client socket!", c->socket);
        s->free_clients[c->id] = true;
        
        printf("#%lld: Connection closed!\n", c->socket);
    }
    
    ZERO_MEMORY(c, sizeof(Request));
}

bool send_to_client(Request *c, String *buffer, s64 at_once = -1)
{
    if (!c->connected) return false;
    if (at_once <= 0) at_once = buffer->count;
    
    // printf("[send/start]: len: %d ; at_once: %d\n", buffer->count, at_once);
    
    s64 remain = buffer->count;
    s64 sent  = 0;
    s32 err   = 0;
    
    while (remain != 0) {
        sent = send(c->socket, buffer->data, at_once, 0);
        err = WSAGetLastError();
        
        if (sent == 0) {
            fprintf(stderr, "Connection is closed!\n");
            return false;
        } else if (err == WSAEWOULDBLOCK) {
            // print("[send/progress]: WSAEWOULDBLOCK\n");
            continue;        
        } else if (sent == SOCKET_ERROR) {
            // It is valid??
            fprintf(stderr, "SOCKET ERROR. Error code: %d\n", WSAGetLastError());
            return false;
        }

        remain -= sent;
        
        // printf("[send/progress]: sent: %d ; remain: %d ; rate: %d\n", sent, remain, buffer->count, at_once);
        
        ASSERT(remain >= 0, "TOTAL FAIL!?");
    }
    
    return true;
}

bool http_parse_header(Request *c)
{
    auto buf_size = sizeof(c->buf);
    
    while (true) {
        int r = recv(c->socket, c->buf, buf_size, 0);
        int err = WSAGetLastError();
        
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
        
        bool found = false;
        String buf = String(c->buf, r);
        c->header = split(buf, CRLF CRLF, &c->body, &found);
        if (c->body.count != 0) {
            printf("\nBODY\n" SFMT "\n\n", SARG(c->body));
            ASSERT(c->body.count == 0, "Probably the body camed with the header? %ld\n", c->body.count);    
        }
        
        if (!found) {
            printf("Not found the end of the http header!\n");
            return false;
        }
        
        printf("\n-------------------\nsocket: #%lld\n" SFMT "\n", c->socket, SARG(c->header));
        
        String line = split_and_move(&c->header, CRLF, &found);
        if (!found) return false;
        {
            String method = split_and_move(&line, " ", &found);
            if (!found) return false;
            
            c->path = split_and_move(&line, " ", &found);
            if (!found) return false;
            
            c->protocol = line;
            if (c->protocol != HTTP_1_1) {
                printf("Invalid protocol -> " SFMT "\n", SARG(c->protocol));
                return false;
            }
            
            c->method = http_method_str_to_enum(method);
        }
        
        do {
            line = split_and_move(&c->header, CRLF, &found);
            
            String key, value;
            if (!http_header_parse_line(line, &key, &value)) {
                fprintf(stderr, "Failed to parse header line -> " SFMT "\n", SARG(line));
                return false;
            }
            
            if (key == "Content-Type") {
                c->content_type = content_type_str_to_enum(value);
                if (c->content_type == Mime_None) {
                    printf("Content type not handled as enum -> " SFMT "\n", SARG(value));
                }
                
            } else if (key == "Content-Length") {
                bool to_int_ok = true;
                c->content_length = string_to_int(value, &to_int_ok);
                
                if (!to_int_ok) {
                    fprintf(stderr, "Failed to parse Content-Length to int -> " SFMT "\n", SARG(value));
                    return false;
                }
            }
            
        } while (found);
        
        c->state = HTTP_STATE_HEADER_PARSED;
        
        return true;
    }
}

inline void http_header_append(String *buf, char *data)
{
    join(buf, data);
    join(buf, CRLF);
}

inline void http_header_finish(String *buf)
{
    join(buf, CRLF CRLF);
}

String http_header_create(Http_Response_Status status)
{
    String h = string_create(4096);

    switch (status) {
        case HTTP_OK:
            http_header_append(&h, HTTP_1_1 " 200 Ok");
        break;
        case HTTP_NOT_FOUND: 
            http_header_append(&h, HTTP_1_1 " 404 Not Found");
        break;
        case HTTP_BAD_REQUEST: 
            http_header_append(&h, HTTP_1_1 " 400 Bad Request");
        break;
        case HTTP_MOVED_PERMANENTLY: 
            http_header_append(&h, HTTP_1_1 " 301 Moved Permanently");
        break;
        case HTTP_SEE_OTHER: 
            http_header_append(&h, HTTP_1_1 " 303 See Other");
        break;
        case HTTP_TEMPORARY_REDIRECT: 
            http_header_append(&h, HTTP_1_1 " 307 Temporary Redirect");
        break;
        case HTTP_PERMANENT_REDIRECT: 
            http_header_append(&h, HTTP_1_1 " 308 Permanent Redirect");
        break;
        default:
            ASSERT(0, "TODO more http header!\n");
    }

    return h;
}

void handle_request(Request *c)
{
    Http_Response_Status status = HTTP_OK;

    if (c->method == HTTP_METHOD_POST) {
        if (c->path == "/upload-photo") {
        }

        status = HTTP_SEE_OTHER;
    } else {
    }
    
    String header = http_header_create(status); 
    http_header_append(&header, "Connection: close");
    if (status == HTTP_TEMPORARY_REDIRECT) {
        http_header_append(&header, "Location: /");
    }

    printf("\nResponse:\n" SFMT " \n", SARG(header));
        
    String body = read_entire_file("index.html", "r");
    {
        char cl[128] = {0};
        snprintf(*&cl, 128, "Content-Length: %ld", body.count);
        http_header_append(&header, cl);
    }
    
    join(&header, CRLF);
    send_to_client(c, &header);
    send_to_client(c, &body);
    
    free(header);
    free(body);
}

void server_listen(Server *s)
{
    printf("\n\nServer listening at %d...\n\n", s->port);
    
    s->running = true;

    fd_set _read_fds, read_fds;
    TIMEVAL _polltime, polltime;
    FD_ZERO(&_read_fds);
    FD_SET(s->socket, &_read_fds);
    _polltime.tv_sec = 10000;
    
    while (s->running) {
        read_fds = _read_fds;
        polltime = _polltime;
        
        int r = select(1, &read_fds, NULL, NULL, &polltime);
        if (r == SOCKET_ERROR) {
            fprintf(stderr, "select() is failed. Error code: %d -> ", WSAGetLastError());
            switch (WSAGetLastError()) {
                case WSAEFAULT:
                    fprintf(stderr, "WSAEFAULT\n");
                break;
                case WSAENETDOWN:
                    fprintf(stderr, "WSAENETDOWN\n");
                break;
                case WSANOTINITIALISED:
                    fprintf(stderr, "WSANOTINITIALISED\n");
                break;                
                case WSAEINVAL:
                    fprintf(stderr, "WSAEINVAL\n");
                break;                
                case WSAEINTR:
                    fprintf(stderr, "WSAEINTR\n");
                break;
                case WSAEINPROGRESS:
                    fprintf(stderr, "WSAEINPROGRESS\n");
                break;
                case WSAENOTSOCK:
                    fprintf(stderr, "WSAENOTSOCK\n");
                break;                
            }
            
            server_shutdown(s, true);
            
            return;
            
        } else if (r == 0) {
            // select() timeout, continue...        
            continue;
        }
    
        Request *c = nullptr;
        for (auto i = 0; i < MAX_CLIENTS; i++) {
            if (s->free_clients[i]) {
                c = s->clients+i;
                c->connected = true;
                s->free_clients[i] = false;
                break;
            }
        }
        
        if (c == nullptr) {
            fprintf(stderr, "No more room to connect!\n");
            continue;
        }
        
        c->socket = accept(s->socket, NULL, NULL);
        if (c->socket == INVALID_SOCKET) {
            fprintf(stderr, "Failed to accept new connection. Error code: %d\n", WSAGetLastError());
            continue;
        }
        
        bool success = http_parse_header(c);
        if (!success) {
            fprintf(stderr, "Failed to parse http header!\n");
            close_client(s, c);
            continue;
        }
        
        // if (c->method == HTTP_METHOD_POST) {
        //     print("POST -> cl: %ld\n", c->content_length);
        //     // @Debug: Just for testing...
        //     if (c->content_length) {
        //         char buf[RECV_BUF_SIZE] = {0};
        //         ZERO_MEMORY(buf, RECV_BUF_SIZE);
        //         snprintf(buf, RECV_BUF_SIZE, ".\\%lld.tmp", c->socket);
        //         FILE *fp = fopen(buf, "wb");
        //         ASSERT(fp, "Failed to create file handler for %s!", buf);
                
        //         auto remain = c->content_length;
        //         while (remain != 0) {
        //             int r = recv(c->socket, buf, RECV_BUF_SIZE, 0);
        //             int err = WSAGetLastError();
                    
        //             if (r == 0) {
        //                 fprintf(stderr, "#%lld: Connection is closed!\n", c->socket);
        //                 return;
        //             } else if (err == WSAEWOULDBLOCK) {
        //                 // print("WSAEWOULDBLOCK\n");
        //                 continue;            
        //             } else if (r == SOCKET_ERROR) {
        //                 fprintf(stderr, "#%lld: SOCKET ERROR. Error code: %d\n", c->socket, WSAGetLastError());
        //                 return;
        //             }
                    
        //             remain -= r;
        //             ASSERT(remain >= 0, "TOTAL FAIL!?");
        //             printf("r: %ld\n", remain);
                    
        //             size_t rr = fwrite(buf, 1, r, fp);
        //             ASSERT(rr == (size_t)r, "#%lld: recv() -> %llu not equal to fwrite() -> %llu results\n", c->socket, (size_t)r, rr);
        //         }
                
        //         printf("[%lld/%p]: body received successfully\n", c->socket, c);
                
        //         ASSERT(fclose(fp) == 0, "Failed to close file handler!");
        //     }            
        // }
        
        handle_request(c);
        close_client(s, c);
    }
}

int main(int argc, char **argv)
{
    Server s;
    int port = 6969;
    bool success = server_create(&s, port);
    ASSERT(success, "Failed to create server! Port: %d\n", port);
    server_listen(&s);

    return 0;
}