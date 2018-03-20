#include "http-client-helper.h"

// Function for onetime curl struct setup
// Doesn't include file stuff
int set_global_opts(CURL *curl, long port) { 
    /* upload to this place */ 
    curl_easy_setopt(curl, CURLOPT_PORT, port);
    /* tell it to "upload" to the URL */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    // Progress callback options
 	//curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, info_callback);
    //curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &curl_progress);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    /* enable verbose for easier tracing */ 
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    return 0;
}

int init_file_upload(CURL *curl, char *local_fn, char *remote_path) {
    struct stat file_info;
    create_full_path(remote_path);
    
    curr_fd = fopen(local_fn, "rb"); /* open file to upload */ 
    if(!curr_fd)
        return -1; /* can't continue */ 
    /* to get the file size */ 
    if(fstat(fileno(curr_fd), &file_info) != 0)
        return -1; /* can't continue */ 
    
    curl_easy_setopt(curl, CURLOPT_URL, full_remote_path);
    /* set where to read from */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, curr_fd);
    /* and give the size of the upload (optional) */ 
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, file_info.st_size);

    return 0;
}

int create_full_path(char *remote_path) {
    char* tmp = (char *)realloc(full_remote_path, strlen(remote_base_url) + strlen(remote_path) + 1);

    if (tmp == NULL)
        return -1;
    // Construct full destination using global base url
    full_remote_path = strcpy(tmp, remote_base_url);
    strcat(full_remote_path, remote_path);
    tmp = NULL;

    return 0;
}

