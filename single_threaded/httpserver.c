#include <stdio.h>
#include "asgn2_helper_funcs.h"
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "request.h"
#include "response.h"
#include <errno.h>
#include <sys/stat.h>

#define BUFSIZE 2050
double min(double a, double b) {
    return (a < b) ? a : b;
}

void closeConnection(
    char *bufRequest, char *bufResponse, Request *request, int responseCode, int client_socket) {
    //special arguments:
    //NULL for any buffer or request that wasn't allocated
    //0 as responseCode if we don't want anything sent

    // We read all the data that is available from the client
    int bytesRead = 0;
    while (1) {
        bytesRead = read(client_socket, bufRequest, BUFSIZE);
        if (bytesRead == -1 || bytesRead == 0)
            break;
    }

    if (responseCode != 0) {
        int response_size = produce_response(bufResponse, responseCode);
        write_n_bytes(client_socket, bufResponse, response_size);
    }

    close(client_socket);

    if (request != NULL)
        free_request(request);

    if (bufResponse != NULL)
        free(bufResponse);

    if (bufRequest != NULL)
        free(bufRequest);
}

int main(int argc, char *argv[]) {
    // Get the port number from command line args
    if (argc != 2) {
        write(STDERR_FILENO, "Invalid number of args\n", 23);
        exit(1); //only here we terminate
    }
    int port = strtol(argv[1], NULL, 10);

    // Initialize a listener socket on the port
    Listener_Socket listener_socket; //THE SERVER SOCKET
    int result = listener_init(&listener_socket, port); 
    if (result == -1) {
        write(STDERR_FILENO, "Invalid Port\n", 13);
        close(listener_socket.fd);
        exit(1); //only here we terminate
    }

    while (1) {
        Request *request = (Request *) calloc(1, sizeof(Request));
        char *bufResponse = (char *) calloc(BUFSIZE, sizeof(char));

        // Accept a new connection on the socket
        int client_socket = listener_accept(&listener_socket); //THE CLIENT SOCKET
        if (client_socket == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                closeConnection(NULL, bufResponse, request, 400, client_socket);
            } else {
                closeConnection(NULL, bufResponse, request, 500, client_socket);
            }
            continue;
        }

        char *bufRequest = (char *) calloc(BUFSIZE, sizeof(char));
        int readbytes = read_request_header(client_socket, bufRequest, BUFSIZE);
        if (readbytes == -1) {
            closeConnection(bufRequest, bufResponse, request, 400, client_socket);
            continue;
        }
        //if the command is put, bufRequest can possibly have some part of the message-body
        int outcome = parse_request_header(bufRequest,
            request); //be careful when you count the start of the message because the \r is a single character not the two chars \ and r
        if (outcome < 0) {
            if (outcome == -1) { //the request was wrong
                closeConnection(bufRequest, bufResponse, request, 400, client_socket);
            } else //something unexpected happened
                closeConnection(bufRequest, bufResponse, request, 500, client_socket);
            continue;
            //we check later if the method is valid
        }

        if (request->version == NULL || strcmp(request->version, "HTTP/1.1") != 0) {
            closeConnection(bufRequest, bufResponse, request, 505, client_socket);
            continue;
        }

        int startContentBuf = outcome;

        // If GET request
        if (request->method != NULL && strcmp(request->method, "GET") == 0) {
            if (request->content_length != -1 || readbytes > startContentBuf) {
                printf("this was the error 2\n");
                closeConnection(bufRequest, bufResponse, request, 400, client_socket);
                continue;
            }

            // Open the uri for reading - or send a default 4** response on failure
            int fd = open(request->uri, O_RDONLY);
            // At this point, we should have already checked that the request is not ill-formatted (you'll decide when to do this when you write the parser)
            if (fd == -1) {
                if (errno == ENOENT) {
                    // File does not exist
                    closeConnection(bufRequest, bufResponse, request, 404, client_socket);
                } else if (errno == EACCES) {
                    // File exists, but is not accessible due to permission issues
                    closeConnection(bufRequest, bufResponse, request, 403, client_socket);
                } else {
                    // Some other error occurred
                    printf("this was the error 3\n");
                    closeConnection(bufRequest, bufResponse, request, 400, client_socket);
                }
                close(fd);
                continue;
            }
            struct stat file_info;
            if (fstat(fd, &file_info) != 0) {
                closeConnection(bufRequest, bufResponse, request, 500, client_socket);
                close(fd);
                continue;
            }

            // Check if it's a directory
            if (S_ISDIR(file_info.st_mode)) {
                closeConnection(bufRequest, bufResponse, request, 403, client_socket); // Forbidden
                close(fd);
                continue;
            }

            // Access the file size from the st_size member of the struct stat
            off_t file_size = file_info.st_size;

            // Send the appropriate response header to the socket (write_n_bytes())
            int response_size = produce_special_get_response(bufResponse, 200, "OK", file_size);
            write_n_bytes(client_socket, bufResponse, response_size);
            // Send the uri's contents to the socket (pass_n_bytes())
            // This is the message-body of the response:
            pass_n_bytes(fd, client_socket, file_size);
            closeConnection(bufRequest, bufResponse, request, 0, client_socket);
            close(fd);
            continue;

        }
        // If PUT request
        else if (request->method != NULL && strcmp(request->method, "PUT") == 0) {
            if (request->content_length == -1) {
                closeConnection(bufRequest, bufResponse, request, 400, client_socket);
                continue;
            }

            struct stat before_stat;
            int file_exists_before = (stat(request->uri, &before_stat) == 0);

            // Check if it's a directory before trying to write
            if (file_exists_before && S_ISDIR(before_stat.st_mode)) {
                closeConnection(bufRequest, bufResponse, request, 403, client_socket); // Forbidden
                continue;
            }

            // Open the uri for writing - or send a default 4** response on failure
            int fd = open(request->uri, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd == -1) {
                if (errno == ENOENT) {
                    // File does not exist
                    closeConnection(bufRequest, bufResponse, request, 404, client_socket);
                } else if (errno == EACCES) {
                    // File exists, but is not accessible due to permission issues
                    closeConnection(bufRequest, bufResponse, request, 403, client_socket);
                } else {
                    // Some other error occurred
                    closeConnection(bufRequest, bufResponse, request, 400, client_socket);
                }
                close(fd);
                continue;
            }

            int numbytestowrite = min(request->content_length, readbytes - startContentBuf);
            // Write any extra content from the buffer to the uri (write_n_bytes())
            int resw = write_n_bytes(fd, bufRequest + startContentBuf, numbytestowrite);
            // Pass the rest of the content unread from the socket to the uri (pass_n_bytes())
            int resp = pass_n_bytes(client_socket, fd, request->content_length - numbytestowrite);
            if (resp == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    closeConnection(bufRequest, bufResponse, request, 400, client_socket);
                } else {
                    closeConnection(bufRequest, bufResponse, request, 500, client_socket);
                }
                close(fd);
                continue;
            }

            if (resw + resp < request->content_length) {
                closeConnection(bufRequest, bufResponse, request, 400, client_socket);
                close(fd);
                continue;
            }

            // Send a default 2** response
            if (!file_exists_before) // The file was created
                closeConnection(bufRequest, bufResponse, request, 201, client_socket);
            else
                closeConnection(bufRequest, bufResponse, request, 200, client_socket);

            close(fd);
            continue;

        } else {
            // Send a default response for unsupported methods
            closeConnection(bufRequest, bufResponse, request, 501, client_socket);
            continue;
        }
    }

    // Close the listener socket
    close(listener_socket.fd);
    return 0;
}
