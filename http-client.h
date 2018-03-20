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

// Curl multi structure
CURLM *cm;
// Curl easy structure
CURL *curl;
struct progress curl_progress;

char *remote_base_url;
char *full_remote_path;

int main(void);
int blocking_send(char *local_fn, char *remote_path, char *host, long port);
int asynch_send(char *local_fn, char *remote_path);
void curl_init(char *host, long port);

#endif /* http_client_h */
