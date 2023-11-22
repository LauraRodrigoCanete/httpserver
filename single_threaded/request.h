#pragma once

#include <stdint.h>
#include <sys/types.h>

typedef struct {
    char *method;
    char *uri;
    char *version;
    long long int content_length;
} Request;

/*Reads data from client_socket into buf until:
1. An error is encountered: returns -1
2. nbytes have been read: returns nbytes
3. in has been closed - read() would return 0: returns the number of bytes read
4. “\r\n\r\n” is somewhere in the buffer: not necessarily the end*/
int read_request_header(int client_socket, char *buf, int bufSize);

// Parse the request line + header fields using regular expressions
// Capture the method, uri, version, (and content-length)
//it returns the start of the message content
int parse_request_header(char *buf, Request *request);

void free_request(Request *request);
