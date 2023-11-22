#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "queue.h"
#include "rwlock.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/stat.h>

volatile sig_atomic_t shutdown_flag = 0;

void signal_handler(int signum) {
    (void) signum;
    shutdown_flag = 1;
}

typedef struct hashtable_entry {
    char *uri;
    rwlock_t *lock;
    struct hashtable_entry *next; // Next entry for collision resolution
} hashtable_entry_t;

#define HASHTABLE_SIZE 100

typedef struct hashtable {
    hashtable_entry_t *entries[HASHTABLE_SIZE];
} hashtable_t;

hashtable_t *uri_lock_hashtable;
queue_t *connection_queue;

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

void hashtable_init(hashtable_t **ht);
unsigned int hash_uri(const char *uri);
void hashtable_add(hashtable_t *ht, const char *uri);
rwlock_t *hashtable_find(hashtable_t *ht, const char *uri);
void hashtable_remove(hashtable_t *ht, const char *uri);
void free_hashtable_entries(hashtable_t *ht);

void *worker_thread_function(void *arg) {
    (void) arg; // so it doesnt complain about unused parameter
    while (!shutdown_flag) {
        uintptr_t connfd;
        if (!queue_pop(connection_queue, (void **) &connfd)) {
            errx(EXIT_FAILURE, "error in popping element to the queue");
        }
        handle_connection(connfd);
        close(connfd);
    }
    return NULL;
}

int main(int argc, char **argv) {
    int threads = 4; // default number of threads
    int opt;
    size_t port;
    char *endptr = NULL;

    // Parsing command-line options
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            threads = (int) strtoul(optarg, &endptr, 10);
            if (*endptr != '\0') {
                warnx("invalid number of threads: %s", optarg);
                return EXIT_FAILURE;
            }
            break;
        default: // In case of unknown options
            //fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("expected port number after options");
        //fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parsing the port number
    port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (*endptr != '\0') {
        warnx("invalid port number: %s", argv[optind]);
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize the connection queue and the rwlock
    connection_queue = queue_new(threads);
    hashtable_init(&uri_lock_hashtable);

    // Create worker threads
    pthread_t worker_threads[threads]; // Array of pthread_t

    for (int i = 0; i < threads; i++) {
        int rc = pthread_create(&worker_threads[i], NULL, worker_thread_function, NULL);
        if (rc) {
            errx(EXIT_FAILURE, "pthread_create failed for worker thread %d", i);
        }
    }

    Listener_Socket sock; //server socket
    listener_init(&sock, port);

    while (!shutdown_flag) {
        int connfd = listener_accept(&sock); //returns client socket
        intptr_t data = connfd;
        if (!queue_push(connection_queue, (void *) data)) {
            errx(EXIT_FAILURE, "error in pushing element to the queue");
        }
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    free_hashtable_entries(uri_lock_hashtable);
    queue_delete(&connection_queue);

    return EXIT_SUCCESS;
}

void handle_connection(int connfd) {

    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) { //there is an error
        conn_send_response(conn, res);
    } else {
        debug("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);

        rwlock_t *lock = hashtable_find(uri_lock_hashtable, conn_get_uri(conn));
        if (lock == NULL) {
            hashtable_add(uri_lock_hashtable, conn_get_uri(conn));
            lock = hashtable_find(uri_lock_hashtable, conn_get_uri(conn));
        }

        if (req == &REQUEST_GET) {
            reader_lock(lock);
            handle_get(conn);
            reader_unlock(lock);
        } else if (req == &REQUEST_PUT) {
            writer_lock(lock);
            handle_put(conn);
            writer_unlock(lock);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

void handle_get(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    debug("handling get request for %s", uri);

    // 1. Open the file.
    int fd = open(uri, O_RDONLY);

    // If  open it returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (hint: check errno for these cases)!

    if (fd < 0) {
        debug("%s: %d", uri, errno);
        if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
            goto out;
        } else if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    // 2. Get the size of the file.
    // (hint: checkout the function fstat)!
    struct stat file_info;
    if (fstat(fd, &file_info) != 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        close(fd);
        goto out;
    }

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (hint: checkout the macro "S_IFDIR", which you can use after you call fstat!)
    if (S_ISDIR(file_info.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        close(fd);
        goto out;
    }

    // 4. Send the file
    // (hint: checkout the conn_send_file function!)

    off_t file_size = file_info.st_size;
    res = conn_send_file(conn, fd, file_size);

    if (res == NULL) {
        res = &RESPONSE_OK;
    }

    close(fd);
    goto ending;
out:
    conn_send_response(conn, res);
ending:
    fprintf(stderr, "%s,%s,%hu,%d\n", "GET", conn_get_uri(conn), response_get_code(res),
        atoi(conn_get_header(conn, "Request-Id") ? conn_get_header(conn, "Request-Id") : "0"));
}

void handle_unsupported(conn_t *conn) {
    debug("handling unsupported request");

    // send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
    fprintf(stderr, "%s,%s,%hu,%d\n", "UNSUPPORTED", conn_get_uri(conn), (uint16_t) 501,
        atoi(conn_get_header(conn, "Request-Id") ? conn_get_header(conn, "Request-Id") : "0"));
}

void handle_put(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    debug("handling put request for %s", uri);

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;
    debug("%s existed? %d", uri, existed);

    // Open the file..
    int fd = open(uri, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) {
        debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }

    close(fd);

out:
    conn_send_response(conn, res);
    fprintf(stderr, "%s,%s,%hu,%d\n", "PUT", conn_get_uri(conn), response_get_code(res),
        atoi(conn_get_header(conn, "Request-Id") ? conn_get_header(conn, "Request-Id") : "0"));
}

void hashtable_init(hashtable_t **ht) {
    *ht = malloc(sizeof(hashtable_t));
    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        (*ht)->entries[i] = NULL;
    }
}

// Hash function for URIs
unsigned int hash_uri(const char *uri) {
    unsigned long hash = 5381;
    int c;
    while ((c = *uri++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % HASHTABLE_SIZE;
}

// Add entry to the hashtable
void hashtable_add(hashtable_t *ht, const char *uri) {
    unsigned int index = hash_uri(uri);

    hashtable_entry_t *entry = malloc(sizeof(hashtable_entry_t));
    entry->uri = strdup(uri);
    entry->lock = rwlock_new(WRITERS, 0);
    entry->next = ht->entries[index];
    ht->entries[index] = entry;
}

// Find entry in the hashtable
rwlock_t *hashtable_find(hashtable_t *ht, const char *uri) {
    unsigned int index = hash_uri(uri);
    hashtable_entry_t *entry = ht->entries[index];

    while (entry != NULL) {
        if (strcmp(entry->uri, uri) == 0) {
            return entry->lock;
        }
        entry = entry->next;
    }
    return NULL;
}

// Remove entry from the hashtable
void hashtable_remove(hashtable_t *ht, const char *uri) {
    unsigned int index = hash_uri(uri);
    hashtable_entry_t *entry = ht->entries[index];
    hashtable_entry_t *prev = NULL;

    while (entry != NULL) {
        if (strcmp(entry->uri, uri) == 0) {
            if (prev == NULL) {
                ht->entries[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            free(entry->uri);
            rwlock_delete(&(entry->lock));
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void free_hashtable_entries(hashtable_t *ht) {
    hashtable_entry_t *entry, *temp;

    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        entry = ht->entries[i];
        while (entry != NULL) {
            temp = entry;
            entry = entry->next;

            free(temp->uri); // Free the URI string
            rwlock_delete(&(temp->lock)); // Properly delete the rwlock
            free(temp); // Free the entry
        }
    }
}
