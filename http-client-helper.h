#ifndef http_client_helper_h
#define http_client_helper_h

#include "http-client.h"

/*
    REQUIRES:
        Valid (initialized) pointer to curl struct
        port number of server

    MODIFIES:
        curl struct pointed to by argument

    RETURNS:
        0 on success
*/
int set_global_opts(CURL *curl, long port);

/*
    REQUIRES:
        Valid (initialized) pointer to curl struct
        local_fn, remote_path

    MODIFIES:
        curr_fd, curl struct

    RETURNS:
        0 on success
        -1 on bad file open, fstat call
*/
int init_file_upload(CURL *curl, char *local_fn, char *remote_path);

/*
    REQUIRES:
        successful completion of curl_init (for remote base url)
        remote path of destination on server

    MODIFIES:
        full_remote_path

    RETURNS:
        0 on success
        -1 for bad realloc
*/
int create_full_path(char *remote_path);

#endif
