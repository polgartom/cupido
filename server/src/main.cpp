#include "core.h"

#define print printf
 
SOCKET createListeningSocket(int port) {
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

bool http_header_parse_line(String line, String *key, String *value)
{
    bool success = true;
    *key = split(line, ": ", value, &success);
    return success;
}

int main() {
    int port = 6969;
 
    // Create a listening socket
    SOCKET listenSocket = createListeningSocket(port);
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
                    
                    print("> buf: %d ; header: %d ; body: %d\n", req.buf.count, req.raw_header.count, req.raw_body.count);
                    req.state = HTTP_STATE_HEADER_PARSED;
                    
                    bool ok = true;
                    String h = req.raw_header;
                    String line; 
                    String key; 
                    String val;
                    line = split(h, CRLF, &h, &ok);
                    assert(ok);
                    
                    // @Todo: Handle first line: GET / HTTP/1.1
                    print("> " SFMT "\n", SARG(line));
                    
                    while (true) {
                        line = split(h, CRLF, &h, &ok);
                        if (!ok) break;

                        // print("line: " SFMT "\n", SARG(line));

                        ok = http_header_parse_line(line, &key, &val);
                        ASSERT(ok, "Failed to parse header line: " SFMT "\n", SARG(line));
                        
                        if (key == "Content-Type") {
                            bool other_half_is_set = false;
                            String other_half;
                            val = split(val, "; ", &other_half, &other_half_is_set);
                            printf("Content-Type: " SFMT " \n", SARG(val));
                            if (other_half_is_set) {
                                // @Todo
                                // printf("Other half of Content-Type: |" SFMT "|\n", SARG(other_half));
                            }
                            
                            if (val == "application/octet-stream") {
                                req.content_type = Mime_OctetStream;
                            } else {
                                printf("Content-Type not supported: " SFMT " \n", SARG(val));
                            }
                            
                        } else if (key == "Content-Length") {
                            bool success = true;
                            String rem;
                            req.content_length = string_to_int(val, &success, &rem);
                            ASSERT(success, "Failed to convert string to int! Line: |" SFMT "|\n", SARG(line));
                            printf("Content-Length: %d\n", req.content_length);
                        }

                        print("> key: |" SFMT "| ; val: |" SFMT "| \n", SARG(key), SARG(val));
                    }
                    
                }
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