#ifndef http_client_helper_h
#define http_client_helper_h

#include "http-client.h"

int set_global_opts(CURL *curl, long port);
int info_callback(void *p, curl_off_t dltotal, curl_off_t dlnow,
                  curl_off_t ultotal, curl_off_t ulnow);
int create_full_path(char *remote_path);
int init_file_upload(CURL *curl, char *local_fn, char *remote_path);

#endif
