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
    assert(s->clients);
    for (auto i = 0; i < MAX_CLIENTS; i++) s->clients[i].id = i;
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
        
        printf("[client(%lld)]: Connection closed!\n", c->socket);
    }
    
    ZERO_MEMORY(c, sizeof(Request));
    
    printf("[request(%p)]: Cleared up!\n", c);
}

bool send_to_client(Request *c, String *buffer, s64 at_once = -1)
{
    if (!c->connected) return false;
    if (at_once <= 0) at_once = buffer->count;
    
    printf("[send/start]: len: %d ; at_once: %d\n", buffer->count, at_once);
    
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
            print("[send/progress]: WSAEWOULDBLOCK\n");
            continue;        
        } else if (sent == SOCKET_ERROR) {
            // It is valid??
            fprintf(stderr, "SOCKET ERROR. Error code: %d\n", WSAGetLastError());
            return false;
        }

        remain -= sent;
        
        printf("[send/progress]: sent: %d ; remain: %d ; rate: %d\n", sent, remain, buffer->count, at_once);
        
        if (remain < 0) {
            ASSERT(remain < 0, "How can this happen?");
            return false;
        }
    }
    
    printf("[send/done]: OK!\n");
    
    return true;
}

bool http_parse_header(Request *c)
{
    auto buf_size = sizeof(c->header_buf);
    
    while (true) {
        int r = recv(c->socket, c->header_buf, buf_size-1, 0);
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
        
        // @Speed
        // String buf = String(c->header_buf);
        // int end = find_index_from_left(buf, CRLF CRLF);
        // if (end == -1) {
        //     printf("Not found the end of the http header, probably isn't arrived. Size: %d\n", r);
        //     return false;
        // }

        bool found = false;
        
        String buf = String(c->header_buf);
        String body;
        String header = split(buf, CRLF CRLF, &body, &found);
        if (!found) {
            printf("Not found the end of the http header!\n");
            return false;
        }
        
        String line = split_and_keep(&header, CRLF, &found);
        if (!found) return false;
        {
            String method = split_and_keep(&line, " ", &found);
            if (!found) return false;
            printf("Method: |" SFMT "|\n", SARG(method));
            
            String path = split_and_keep(&line, " ", &found);
            if (!found) return false;
            printf("Path: |" SFMT "|\n", SARG(path));
            
            String protocol = line;
            printf("Protocol: |" SFMT "|\n", SARG(protocol));
            
            printf("\n-------------\n");
        }
        
        do {
            line = split_and_keep(&header, CRLF, &found);
            
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
                    fprintf(stderr, "Failed to parse content length value to int -> " SFMT "\n", SARG(value));
                    return false;
                }
            }
            
        } while (found);
        
        print("\n\n---HTTP_HEADER--------\n\n");
        print(SFMT, SARG(header));
        print("\n\n---HTTP_HEADER_END----\n\n");
    
        print("> buf: %d ; header: %d ; body: %d\n", buf.count, header.count, body.count);
    
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
        default:
            ASSERT(0, "TODO more http header!\n");
    }

    return h;
}

void server_listen(Server *s)
{
    printf("Server listening at %d...\n", s->port);
    
    s->running = true;

    fd_set _read_fds, read_fds;
    TIMEVAL _polltime, polltime;
    FD_ZERO(&_read_fds);
    FD_SET(s->socket, &_read_fds);
    _polltime.tv_sec = 10;
    
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
        
        String header = http_header_create(HTTP_OK);
        http_header_append(&header, "Content-Length: 0");
        http_header_append(&header, "Connection: close");
        http_header_finish(&header);
        
        print("\n\n---RESPONSE--------\n\n");
        print(SFMT, SARG(header));
        print("\n\n---RESPONSE_END--------\n\n");
        
        send_to_client(c, &header);        
        close_client(s, c);
        
        free(&header);
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