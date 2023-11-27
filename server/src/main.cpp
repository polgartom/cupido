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

bool init_server(Server *s, int port)
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
 
    // @Temporary?
    FD_ZERO(&s->waiting);
    FD_SET(s->socket, &s->waiting);
    s->waiting_ttl.tv_sec = 60;
    
    return true;
}

inline bool http_header_parse_line(String line, String *key, String *value)
{
    bool success = true;
    *key = split(line, ": ", value, &success);
    return success;
}

inline void close_client_connection(Server *s, Request *c)
{
    if (c->connected) {
        closesocket(c->socket);
        s->free_clients[c->id] = true;
    }
    ZERO_MEMORY(c, sizeof(Request));
}

bool send(Request *c, String *buffer, s64 at_once = -1)
{
    if (!c->connected) return false;
    if (at_once <= 0) at_once = buffer->count;
    
    s64 remain = buffer->count;
    s64 sent  = 0;
    s32 err   = 0;
    
    while (remain != 0) {
        sent = send(c->socket, buffer->data, at_once, 0);
        err = WSAGetLastError();
        print("> sent: %d ; err: %d\n", sent, err);
        
        if (sent == 0) {
            fprintf(stderr, "Connection is closed!\n");
            return false;
        } else if (err == WSAEWOULDBLOCK) {
            print("[send]: WSAEWOULDBLOCK\n");
            continue;        
        } else if (sent == SOCKET_ERROR) {
            // It is valid??
            fprintf(stderr, "SOCKET ERROR. Error code: %d\n", WSAGetLastError());
            return false;
        }
    
        printf("[send]: progress: %d/%d ; rate: %d\n", remain, buffer->count, at_once);
    
        remain -= sent;
        if (remain < 0) {
            ASSERT(remain < 0, "How can this happen?");
            return false;
        }
    }
    
    printf("[send]: DONE!\n");
    
    return true;
}

bool http_parse_header(Request *c)
{
    while (1) {
        int r = recv(c->socket, c->buf, 4096-1, 0);
        int err = WSAGetLastError();
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
        
        String buf = String(c->buf, r);
        int end = find_index_from_left(buf, CRLF CRLF);
        if (end == -1) {
            printf("Not found the end of the http header, probably isn't arrived. Size: %d\n", r);
            return false;
        }
        
        String body;
        String header = split(buf, CRLF CRLF, &body);
        
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

String http_header_create(Http::Response_Status status)
{
    String h;
    alloc(&h, 4096);
    switch (status) {
        case Http::OK:
            http_header_append(&h, HTTP_1_1 " 200 Ok");
        break;
        case Http::NOT_FOUND: 
            http_header_append(&h, HTTP_1_1 " 404 Not Found");
        break;
        case Http::BAD_REQUEST: 
            http_header_append(&h, HTTP_1_1 " 400 Bad Request");
        break;
        default:
            ASSERT(0, "TODO more http header!\n");
    }

    String x;
    
    return h;
}

#define SERVER_WAIT_FOR_INCOMMING_CONNECTIONS(__server) \
    do {\
        int __sr = select(1, &__server->waiting, NULL, NULL, &__server->waiting_ttl); \
        if (__sr == SOCKET_ERROR) { \
            fprintf(stderr, "select() is failed. Error code: %d\n", WSAGetLastError()); \
            continue; \
        } else if (__sr == 0) { \
            fprintf(stderr, "What is this?\n"); \
            continue; \
        } \
    } while(0); \

void server_listen(Server *s)
{
    while (true) {
        SERVER_WAIT_FOR_INCOMMING_CONNECTIONS(s);

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
            close_client_connection(s, c);
            continue;
        }
        
        String header = http_header_create(Http::NOT_FOUND);
        http_header_append(&header, "Content-Length: 0");
        http_header_finish(&header);
        
        print("\n\n---RESPONSE--------\n\n");
        print(SFMT, SARG(header));
        print("\n\n---RESPONSE_END--------\n\n");
        
        send(c, &header);
        free(&header);
        
        close_client_connection(s, c);
    }
}

int main() 
{
    Server s;
    int port = 6969;
    bool success = init_server(&s, port);
    ASSERT(success, "Failed to create server! Port: %d\n", port);
    printf("Server listening at %d...\n", port);

    server_listen(&s);

    closesocket(s.socket);
    WSACleanup();

    return 0;

    // while (true) {

    //     // FD_ZERO(&list);
    //     // FD_SET(clientSocket, &list);


    //     Request *req = req_bucket+i;
    //     req->id = i; // @Todo: set before this
    //     req->connected = true;
    //     req->socket = clientSocket;
        
    //     print("New connection! req: [%p] ; socket: %lld\n", req, req->socket);

    //     while (true) {
    //         int err;
    //         int r = recv(req->socket, &*req->buf, 4096-1, 0);
    //         err = WSAGetLastError();
            
    //         if (r == 0) {
    //             fprintf(stderr, "Connection is closed!");
    //             return false;
    //         } else if (err == WSAEWOULDBLOCK) {
    //             // print("WSAEWOULDBLOCK\n");
    //             continue;            
    //         } else if (r == SOCKET_ERROR) {
    //             std::cerr << "SOCKET ERROR. Error code: " <<  WSAGetLastError() << std::endl;
    //             return false;
    //         }
            
    //         if (req->state != HTTP_STATE_HEADER_PARSED) {
    //             auto buf = String(req->buf, r);
    //             int end = find_index_from_left(buf, CRLF CRLF);
    //             if (end == -1) {
    //                 printf("Not found the end of the http header, probably isn't arrived. Size: %d\n", r);
    //                 break;
    //             }
                
    //             String body;
    //             String header = split(buf, CRLF CRLF, &body);
                
    //             print("\n\n---HTTP_HEADER--------\n\n");
    //             print(SFMT, SARG(header));
    //             print("\n\n---HTTP_HEADER_END----\n\n");
    
    //             print("> buf: %d ; header: %d ; body: %d\n", buf.count, header.count, body.count);

    //             req->state = HTTP_STATE_HEADER_PARSED;
                
    //             print("exit...\n");
    //             exit(0);
    //         }
    //     }

        // while (true) {
        //     int err;
        //     ZERO_MEMORY(tmp_buf, sizeof(tmp_buf));
            
        //     // int sr = select(1, &list, NULL, NULL, &tt);
        //     // err = WSAGetLastError();
        //     int r = recv(req.socket, tmp_buf, 4096-1, 0);
        //     err = WSAGetLastError();
        //     // print("> recv: %d ; err: %d\n", r, err);
            
        //     if (r == 0) {
        //         fprintf(stderr, "Connection is closed!");
        //         return false;
        //     } else if (err == WSAEWOULDBLOCK) {
        //         // print("WSAEWOULDBLOCK\n");
        //         continue;            
        //     } else if (r == SOCKET_ERROR) {
        //         fprintf(stderr, "SOCKET ERROR - Connection is closed!");
        //         std::cerr << "SOCKET ERROR. Error code: " <<  WSAGetLastError() << std::endl;
        //         return false;
        //     }

        //     join(&req.buf, tmp_buf);
            
        //     if (!(req.state & HTTP_STATE_HEADER_PARSED)) {
        //         // Locate end of the http header    
        //         int end = find_index_from_left(req.buf, CRLF CRLF);
        //         if (end != -1) {
        //             req.raw_header = chop(req.buf, end, &req.raw_body);
        //             req.raw_body = advance(req.raw_body, strlen(CRLF CRLF));
                    
        //             print("\n\n---HTTP_HEADER--------\n\n");
        //             print(SFMT, SARG(req.raw_header));
        //             print("\n\n---HTTP_HEADER_END----\n\n");

        //             print("> buf: %d ; header: %d ; body: %d\n", req.buf.count, req.raw_header.count, req.raw_body.count);
        //             req.state = HTTP_STATE_HEADER_PARSED;
                    
        //             bool ok = true;
        //             bool has_more_line = true;
        //             String h = req.raw_header;
        //             String line; 
        //             String key; 
        //             String val;
        //             line = split(h, CRLF, &h, &has_more_line);
        //             assert(has_more_line);
                    
        //             if (string_starts_with_and_step(&line, "GET ")) {
        //                 req.method = Http_Method_Get;
        //             } else if (string_starts_with_and_step(&line, "POST ")) {
        //                 req.method = Http_Method_Post;
        //             } else if (string_starts_with_and_step(&line, "DELETE ")) {
        //                 req.method = Http_Method_Delete;
        //             } else {
        //                 ASSERT(0, "Http method not supported! -> " SFMT "\n", SARG(line));
        //             }
                    
        //             {
        //                 req.path = split(line, " ", &line, &ok);
        //                 ASSERT(ok, "Invalid http header! Malformed path. -> " SFMT "\n", SARG(line)); // can be?
        //                 ASSERT(line == HTTP_1_1, "Invalid http header! Protocol not supported. -> " SFMT "\n", SARG(line));
        //                 req.protocol = line;
        //             }
                    
        //             while (has_more_line) {
        //                 line = split(h, CRLF, &h, &has_more_line);
        //                 ASSERT(line.count != 0, "Invalid http header!"); // can be?

        //                 ok = http_header_parse_line(line, &key, &val);
        //                 ASSERT(ok, "Failed to parse header line: " SFMT "\n", SARG(line));
                        
        //                 if (key == "Content-Type") {
        //                     bool other_half_is_set = false;
        //                     String other_half;
        //                     val = split(val, "; ", &other_half, &other_half_is_set);
        //                     if (other_half_is_set) {
        //                         // @Todo
        //                         // printf("Other half of Content-Type: |" SFMT "|\n", SARG(other_half));
        //                     }
                            
        //                     req.content_type = content_type_str_to_enum(val);
        //                     if (req.content_type == Mime_None) {
        //                         printf("Content-Type not supported: " SFMT " \n", SARG(val));
        //                     }
                            
        //                     // printf("Content-Type: " SFMT " (%d)\n", SARG(val), req.content_type);
        //                 } 
        //                 else if (key == "Content-Length") {
        //                     req.content_length = string_to_int(val);
        //                     ASSERT(errno == 0, "Failed to convert string to int! Line: |" SFMT "|\n", SARG(line));
        //                     // printf("Content-Length: %d\n", req.content_length);
        //                 }

        //                 print("> key: |" SFMT "| ; val: |" SFMT "| \n", SARG(key), SARG(val));
        //             }
                    
        //         }
            
        //         print("");
        //     } 
        //     else if (req.state & HTTP_STATE_HEADER_PARSED) {
                
        //     }
            
            
        // }   
 
        // Close the client socket
        // closesocket(clientSocket);
        // print("Connection closed! req: [%p] ; fd: %lld\n", req, req->socket);
    // }
 
    // Close the listening socket
    // closesocket(listenSocket);
 
    // Cleanup Winsock
    // WSACleanup();
}