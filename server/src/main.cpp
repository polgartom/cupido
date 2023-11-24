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
 
    return listenSocket;
}

#define EXIT() \
    closesocket(clientSocket); \
    closesocket(listenSocket); \
    WSACleanup(); \
    return 1;

bool read_socket(Request *req)
{
    ZERO_MEMORY(req->buf, 4096);
    req->buf_len = recv(req->socket, req->buf, 4096-1, 0);
    
    if (r == 0) {
        fprintf(stderr, "Connection is closed!");
        return false;
    } else if (r == SOCKET_ERROR) {
        fprintf(stderr, "SOCKET ERROR - Connection is closed!");
        return false;
    }
    
    return true;
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
 
    // Wait for incoming connections
    while (true) {
        // Accept a new connection
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to accept connection. Error code: " << WSAGetLastError() << std::endl;
            EXIT();
        }
 
        // Handle the connection (e.g., receive and send data)
        Request req = {0};
        req.socket = clientSocket; 
        
        if (!read_socket(&req)) {
            EXIT();
        }

        auto buffer = String(req->buf);
        while (buffer.count) {
            int index = find_index_from_left(buffer, CRLF);
            if (index == -1) {
                // Probably this is invalid at this point
                ASSERT(index != -1, "Invalid header!\n");
                break;
            }
            
            auto line = chop(buffer, index);
            // print("[line]: |" SFMT "|\n", SARG(line));
            
            if (string_starts_with(line, "Content-Type: ")) {
                auto content_type = advance(line, strlen("Content-Type: "));
                auto end_index = find_index_from_left(content_type, ";");
                if (end_index != -1) {
                    content_type.count = end_index;
                }
                if (content_type == "application/octet-stream") {
                    req.content_type = Mime_OctetStream;
                } else {
                    printf("Content-Type not supported " SFMT " \n", SARG(content_type));
                }
                
                print("[content_type]: |" SFMT "| enum: %d\n", SARG(content_type), req.content_type);
            }
            else if (string_starts_with(line, "Content-Length: ")) {
                auto len = advance(line, strlen("Content-Length: ")); 
                auto rem = String();
                bool success = false;
                req.content_length = string_to_int(len, &success, &rem);
                ASSERT(success, "Invalid Content-Length. Given: " SFMT "\n", SARG(len));
            }
            
            buffer = advance(buffer, index+CRLF_LEN);
            index = find_index_from_left(buffer, CRLF);
            if (index == 0) {
                req.flags |= HEADER_PARSED;
                buffer = advance(buffer, CRLF_LEN);
                print("remained_buf_size: %d ; req.content_type: %d ; req.content_lenght: %d\n", buffer.count, req.content_type, req.content_length);
                break;
            }
        }        
        
        // int r;
        // while (r = recv(clientSocket, req.buf, 4096-1, 0)) {
        //     if (r > 0) {
        //         if (req.flags == 0) {
        //             // Parse header
        //         }
        //         else if (req.flags & HEADER_PARSED) {
        //             // Parse body
        //             print("parse body!\n");
                    
        //             break;                    
        //         }
        //     } else if (r == 0) {
        //         std::cout << "Connection is closed!" << std::endl;
        //         break;
        //     } else if (r == SOCKET_ERROR) {
        //         std::cerr << "SOCKET ERROR - Connection is closed!" << std::endl;
        //         break;
        //     }
            
        // }
        
        // if (r > 0) {
        //     String s_buf(buf);
        //     int pos = string_contains(s_buf, "\r\n");
        //     if (pos != -1) {
        //         String remain = advance(s_buf, pos+strlen("\r\n"));
        //         s_buf.count = pos; 
        //         print("[line]:   |%s|\n", to_cstr(s_buf));
        //         print("[remain]: |%s|\n", to_cstr(remain));
        //     } else {
        //         print("Invalid header sent!\n");
        //     }

        //     printf("\n\n---------------------------\n\n");
        // }
        // if (r == 0) {
        // } else if (r == SOCKET_ERROR) {
        // }
 
        // Close the client socket
        closesocket(clientSocket);
    }
 
    // Close the listening socket
    closesocket(listenSocket);
 
    // Cleanup Winsock
    WSACleanup();
 
    return 0;
}