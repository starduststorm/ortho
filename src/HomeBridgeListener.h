#ifndef HOMEBRIDGELISTENER_H
#define HOMEBRIDGELISTENER_H

#include <unistd.h> 
#include <stdio.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <fcntl.h>
#include <sys/errno.h>

#include <string>
#include <sstream>
#include <iostream>

#define PORT 8080

enum RemoteCommand {
    none, on, off
};

class HomeBridgeListener {
private:
    int server_fd; 
    struct sockaddr_in address; 
    int sockopt = 1; 
    int addrlen = sizeof(address); 
public: 
    HomeBridgeListener() {
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }
        
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, 
                                                      &sockopt, sizeof(sockopt))) { 
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if (0 != fcntl(server_fd, F_SETFL, O_NONBLOCK)) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }

        address.sin_family = AF_INET; 
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(PORT);
        
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
    }

    ~HomeBridgeListener() {
        if (0 != close(server_fd)) {
            perror("close");
        }
    }

     RemoteCommand poll(bool status) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
            if (errno == EAGAIN) {
                return none;
            }
            perror("accept");
            exit(EXIT_FAILURE);
        }
        char buffer[1024] = {0};
        int read_status = read(new_socket, buffer, 1024);
        // printf("--\n%s\n--\n", buffer);

        // Here we interpret a restful HTTP request with a janky restful http parsing.
        // Need to support GET and POST

        /*
        GET /api/status HTTP/1.1
        host: 127.0.0.1:8080
        */

        std::string kGetStateRequest("GET /api/status HTTP/");
        std::string request(buffer);
        auto res = std::mismatch(kGetStateRequest.begin(), kGetStateRequest.end(), request.begin());

        if (res.first == kGetStateRequest.end()) {
            // Made a status request!
            printf("Received a status request from HomeBridge!\n");
            const char *status_str = (status ? "true" : "false");
            char response[128];
            snprintf(response, 128, "HTTP/1.1 200 OK\nContent-Length: %i\nContent-Type: application/json\n\n%s", (unsigned int)strlen(status_str), status_str);
            send(new_socket, response,  strlen(response), 0);
            // printf("Response:\n--\n%s\n--\n", response);
        }

        /*
        POST /api/order HTTP/1.1
        Content-type: application/json
        host: 127.0.0.1:8080
        content-length: 20
        Connection: close

        {"targetState":true}
        */

        RemoteCommand command = none;
        std::string kSetStateRequest("POST /api/order HTTP/");
        res = std::mismatch(kSetStateRequest.begin(), kSetStateRequest.end(), request.begin());

        if (res.first == kSetStateRequest.end()) {
            // Made a state-change request!
            printf("Received a set-state request from HomeBridge!\n");

            const char *response;
            const char *target_str = "targetState\":";
            size_t pos = request.find(target_str, 0);
            if (pos != std::string::npos) {
                pos += strlen(target_str);
                if (request.compare(pos, 4, "true") == 0) {
                    command = on;
                    response = "HTTP/1.1 200 OK\n\n";
                } else if (request.compare(pos, 5, "false") == 0) {
                    command = off;
                    response = "HTTP/1.1 200 OK\n\n";
                }
            }
            if (command == none) {
                response = "HTTP/1.1 400 BAD REQUEST\n\n";
            };

            send(new_socket, response,  strlen(response), 0);
            // printf("Response:\n--\n%s\n--\n", response);
        }

        if (0 != close(new_socket)) {
            perror("close");
            exit(EXIT_FAILURE);
        }
        printf("Done polling\n");
        return command;
    }
};

#endif
