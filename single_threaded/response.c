#include "asgn2_helper_funcs.h"
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "response.h"

#define BUFSIZE 2050 //should be the same as the one in httpserver

typedef struct {
    const char *status;
    const char *message;
    int content_length;
} StringPair;

typedef struct {
    int key;
    StringPair value;
} DictionaryEntry;

static DictionaryEntry dictionary[] = { { 200, { "OK", "OK\n", 3 } },
    { 201, { "Created", "Created\n", 8 } }, { 400, { "Bad Request", "Bad Request\n", 12 } },
    { 403, { "Forbidden", "Forbidden\n", 10 } }, { 404, { "Not Found", "Not Found\n", 10 } },
    { 500, { "Internal Server Error", "Internal Server Error\n", 22 } },
    { 501, { "Not Implemented", "Not Implemented\n", 16 } },
    { 505, { "Version Not Supported", "Version Not Supported\n", 22 } } };

const DictionaryEntry *getValue(int key) {
    for (int i = 0; i < (int) (sizeof(dictionary) / sizeof(dictionary[0])); i++) {
        if (dictionary[i].key == key) {
            return &dictionary[i];
        }
    }
    return NULL;
}

//you then access it like:
/*
const char* status = pair->status;
const char* message = pair->message;
int content_length = pair->content_length;
*/

int produce_response(char *bufResponse, int code) {
    //When you call snprintf with a larger buffer size than the content you write, snprintf pads the remaining space with null characters
    //and ensures that the resulting string is null-terminated. If the content exceeds the specified size, it will be truncated,
    //and the null-terminator will be placed at the specified size or one less to prevent buffer overflows.
    snprintf(bufResponse, BUFSIZE, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n%s", code,
        getValue(code)->value.status, getValue(code)->value.content_length,
        getValue(code)->value.message);

    return strlen(
        bufResponse); //the number of characters in the string until the null-terminating character ('\0') without counting it
}

//acuerdate de controlar el timeout si usas las funciones read_n o pass_n (hay que pensar si aqui o en el server)

int produce_special_get_response(
    char *bufResponse, int code, char *status, long long int content_length) {
    //NO PRODUCE MENSAJE, se hace luego en el server con el pass_n_bytes
    //content_length es un valor para esa clave
    snprintf(bufResponse, BUFSIZE, "HTTP/1.1 %d %s\r\nContent-Length: %lld\r\n\r\n", code, status,
        content_length);

    return strlen(bufResponse);
}
