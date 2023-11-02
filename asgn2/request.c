#include "asgn2_helper_funcs.h"
#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "request.h"
#include <errno.h>
#include <stdint.h>
#include <regex.h>

double max(double a, double b) {
    return (a > b) ? a : b;
}

//the regular expression matching already checks the need for the \r\n so we only need to check it at the end for the last \r\n
static const char *const reLine
    = "([a-zA-Z]{1,8}) +(/[a-zA-Z0-9.-]{1,63}) +(HTTP/[0-9]\\.[0-9])\r\n";
static const char *const reHeader = "([a-zA-Z0-9.-]{1,128}): ([ -~]{0,128})\r\n";

int read_request_header(int client_socket, char *buf, int bufSize) {
    //Read the header (request line + header fields) from the socket into a buffer
    //the request header has to be read with read
    //check the data as it reads to see if it has
    //received the full request header from a client
    int bytesInput = read(client_socket, buf, bufSize);
    int aux = bytesInput; // To check when we read the character EOF
    int leave = 0;
    //check the number of \ needed
    while (leave == 0 && bytesInput < bufSize && bytesInput > 0) {
        if (aux == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return -1;
        }
        if (aux == 0) {
            break;
        }
        for (int i = max(0, bytesInput - aux - 3); i < bytesInput - 3;
             i++) { //el max es por si el \r\n\r\n se corta entre dos reads
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
                leave = 1;
                break;
            }
        }
        if (leave == 0) {
            aux = read(client_socket, buf + bytesInput, bufSize - bytesInput);
            bytesInput += aux;
        }
    }
    return bytesInput;
    //if the command is put, buf can possibly have some part of the message-body
}

//the request received as a parameter is already allocated and initialized to null (calloc)
//returns -1 for an error with the request format and -2 for others related to the system
int parse_request_header(char *buf, Request *request) {

    char *s = buf;
    regex_t regexLine, regexHeader;
    regmatch_t pmatchL[4], pmatchH[3];
    regoff_t len;
    if (regcomp(&regexLine, reLine, REG_NEWLINE | REG_EXTENDED)) {
        return -2;
    }

    int out = regexec(&regexLine, s, 4, pmatchL, 0);
    if (out == REG_NOMATCH) {
        regfree(&regexLine);
        return -1;
    }
    if (out != 0) {
        regfree(&regexLine);
        return -2;
    }

    for (int j = 0; j < 4; ++j) {
        if (pmatchL[j].rm_so == -1) {
            regfree(&regexLine);
            return -1; //en este caso nunca deberíamos entrar aqui pq todas las subexpresiones son obligatorias (no tienen * ni ?) así que para matchear la string entera hay que matchear cada substrings (no siempre es el caso)
        }
    }

    if (pmatchL[0].rm_so
        != 0) { //the is trash before the request, it doesn't begin at the start of buf
        regfree(&regexLine);
        return -1;
    }

    // off = pmatch[1].rm_so + (s - buf); me da el offset desde el principio del buf
    len = pmatchL[1].rm_eo - pmatchL[1].rm_so;
    request->method
        = (char *) calloc(len + 1, sizeof(char)); //including space for the null terminator
    strncpy(request->method, s + pmatchL[1].rm_so,
        len); // Copy the specified number of characters from buf to method, if the length of src is less than n, strncpy() writes  additional  null  bytes  to dest to ensure that a total of n bytes are written.

    len = pmatchL[2].rm_eo - pmatchL[2].rm_so;
    request->uri = (char *) calloc(len + 1, sizeof(char));
    strncpy(request->uri, s + 1 + pmatchL[2].rm_so,
        len - 1); //we don't want the first / of the path to be part of the uri, ex: we want foo.txt not /foo.txt

    len = pmatchL[3].rm_eo - pmatchL[3].rm_so;
    request->version = (char *) calloc(len + 1, sizeof(char));
    strncpy(request->version, s + pmatchL[3].rm_so, len);
    s += pmatchL[0]
             .rm_eo; //nos saltamos el match completo, incluidos los \r\n del final de la requestLine

    if (regcomp(&regexHeader, reHeader, REG_NEWLINE | REG_EXTENDED)) {
        regfree(&regexLine);
        return -2;
    }

    request->content_length = -1; //inicializacion de que no ha encontrado una key content-length
    int contentlengkey = 0; // false pq todavia no ha encontrado una key content_length
    for (int i = 0;; i++) {

        //me busca una substring que matchee la regular expresion dentro de la grande string buf, a partir de la posicion s pero no teniendo que empezar ahi
        out = regexec(&regexHeader, s, 3, pmatchH, 0);
        if (out == REG_NOMATCH)
            break;
        else if (out != 0) {
            regfree(&regexLine);
            regfree(&regexHeader);
            return -2;
        }

        if (pmatchH[0].rm_so
            != 0) { //the is trash before this heading, it doesn't begin at the start of s (where we left off)
            regfree(&regexLine);
            regfree(&regexHeader);
            return -1;
        }

        for (
            int j = 0; j < 3;
            ++j) //si este match no tiene key (no debería pasar) o valor (es posible pq hemos puesto 0 caracteres como minimo) no nos interesa explorarlo
            if (pmatchH[j].rm_so == -1)
                break;

        len = pmatchH[1].rm_eo - pmatchH[1].rm_so;
        char *key = (char *) calloc(len + 1, sizeof(char));
        strncpy(key, s + pmatchH[1].rm_so, len);
        if (strcmp(key, "Content-Length") == 0) {
            if (contentlengkey == 1) { //there can't be multiple content_length keys
                regfree(&regexLine);
                regfree(&regexHeader);
                return -1;
            }
            contentlengkey = 1;
            len = pmatchH[2].rm_eo - pmatchH[2].rm_so;
            char *stringcontentlength = (char *) calloc(len + 1, sizeof(char));
            strncpy(stringcontentlength, s + pmatchH[2].rm_so, len);
            char *endptr;
            long val = strtol(stringcontentlength, &endptr,
                10); //& is because you are providing a pointer to your pointer so strtol can update it
            if (*endptr
                == '\0') //conversion was successful, it converted the chars to ints until the end of the string
                request->content_length = val;
            free(stringcontentlength);
        }
        free(key);

        s += pmatchH[0].rm_eo; // pmatchH[0] pq nos saltamos el match completo incluidos los \r\n
    }

    if (*s != '\r' || *(s + 1) != '\n') {
        regfree(&regexLine);
        regfree(&regexHeader);
        return -1; //comprobamos que al final hay \r\n
    }

    //nos olvidamos del message body y leemos hasta el \r\n
    //da igual que haya más bytes despues del valid request (solo 1 request por connection)

    regfree(&regexLine);
    regfree(&regexHeader);
    return (
        int) (s - buf
              + 2); //it should return the start of the message content (in case it's a put), s-buf gives me la longitud recorrida. we only add 2 because \r\n are 2 chars
}

void free_request(Request *request) {
    free(request->method);
    free(request->uri);
    free(request->version);
    free(request);
}
