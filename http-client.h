#ifndef http_client_h
#define http_client_h

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_WAIT_MSECS 30*1000
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL 3

struct progress {
    double lastruntime;
    CURL *curl;
};

struct req_info {
    char *local_fn;
    char *remote_path;
};

// Curl multi structure
CURLM *cm;
// Curl easy structure
CURL *curl;
struct progress curl_progress;

FILE *curr_fd;

char *remote_base_url;
char *full_remote_path;

pthread_mutex_t in_progress_lock;

int blocking_send(char *local_fn, char *remote_path, char *host, long port);
int asynch_send(char *local_fn, char *remote_path);
int curl_init(char *host, long port);
int curl_destroy();

#endif /* http_client_h */
