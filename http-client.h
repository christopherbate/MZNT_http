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
#include "sds.h"

#define MAX_WAIT_MSECS 30*1000
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL 3

/*
    REQUIRES: 
        Successful completion of curl_init()
        local_fn for local filename (can be relative or absolute path)
        f_offset is requested offset into file, from beginning
        remote_path is path on server, not including host or port 
        
    MODIFIES: 
        in_progress to indicate transfer in progress
    
    RETURNS:
        0 on success
        -1 if other transfer already in progress, failed file init
*/
int asynch_send(char *filename, curl_off_t f_offset, char *rem_path);

/*
    REQUIRES:
        asynch_send must create thread to run send_worker
        arg, cast to req_info struct. Contains local and remote path info

    MODIFIES:
        cm, through curl_multi add and remove handle. Reentrant
        in_progress, cancel_flag for flagging transfer status
        curl, curr_fd, full_remote_path through init_file_upload
    
    RETURNS:
        -1 if exiting prematurely. Detatched thread, returns will not be received
*/
//void *send_worker(void *arg);

/*
    REQUIRES:
        No previous curl_init call, or must be preceded by curl_destroy
        host, port as parameters of server

    MODIFIES:
        Essentially every global var, including curl, cm, remote_port, send_lock
    
    RETURNS:
        0 on success
        -1 given failed cm, curl initialization
*/
int curl_init(char *host, long port);

/*
    REQUIRES:
        Successful completion of curl_initi

    MODIFIES:
        curl, cm, full_remote_path (freeing)
    
    RETURNS:
        0 on success
        -1 if transfer in progress
*/
int curl_destroy();

/*
    REQUIRED:
        Successful completion of curl_init

    MODIFIES:
        Nothing
    
    RETURNS:
        Number of bytes written to this point on success
            NOTE: can legally be equal to num bytes in file
        -1 if transfer not currently in progress
*/
curl_off_t status_send();

/*
    REQUIRED:
        Successful completion of curl_init

    MODIFIES:
        cancel_flag
    
    RETURNS:
        0 on success (NOTE: cancel currently not guaranteed to actually cancel,
                      will only cancel a transfer actually occuring)
*/
int cancel_send();

/*
    REQUIRED:
        Successful completion of curl_init

    MODIFIES:
        N/A
    
    RETURNS:
        pointer to static error string
*/
char* get_error_string();

#endif /* http_client_h */
