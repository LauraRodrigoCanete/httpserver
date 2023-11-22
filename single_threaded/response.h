#pragma once

#include <stdint.h>
#include <sys/types.h>

//this function allocates memory for the buffer depending
//on the size that the code needs and return the buffer
//full with the response and returns the response size
int produce_response(char *bufResponse, int code);

//This request has no message, it is done later on the server with 'pass_n_bytes'
int produce_special_get_response(
    char *bufResponse, int code, char *status, long long int content_length);
