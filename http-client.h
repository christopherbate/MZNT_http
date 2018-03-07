#ifndef http_client_h
#define http_client_h

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

#define MAX_WAIT_MSECS 30*1000
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL 3

struct progress {
    double lastruntime;
    CURL *curl;
};

int main(void);
int blocking_send(char *local_fn, char *remote_path, char *host, long port);
int asynch_send(char *local_fn, char *remote_path, char *host, long port);
void init_single(CURL *curl, FILE *local_fd, long port, char *dest, curl_off_t f_size, struct progress *prog);
int info_callback(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
void create_full_path(char *remote_path, char *host, char **dest);

#endif /* http_client_h */
