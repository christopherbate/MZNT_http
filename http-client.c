#include "http-client.h"

int main(int argc, char **argv) {
    return blocking_send("test.bin", "output.bin", "http://localhost/", 8888);
    //return asynch_send("test.bin", "output.bin", "http://localhost/", 8888);
}

// Can possibly move curl init code out, allow for reusing connection
int blocking_send(char *local_fn, char *remote_path, char *host, long port) {
    CURL *curl;
    CURLcode res;
    struct stat file_info;
    double speed_upload, total_time;

    FILE *fd;
    fd = fopen(local_fn, "rb"); /* open file to upload */ 
    if(!fd)
        return 1; /* can't continue */ 

    /* to get the file size */ 
    if(fstat(fileno(fd), &file_info) != 0)
        return 1; /* can't continue */ 

    curl = curl_easy_init();

    // Construct full destination
    char *dest;
    create_full_path(remote_path, host, &dest);

    if(curl) {
        printf("here\n");
        init_single(curl, fd, port, dest, (curl_off_t)file_info.st_size);
        printf("there\n");
        res = curl_easy_perform(curl);
        /* Check for errors */ 
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));

        }
        else {
            /* now extract transfer info */ 
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

            fprintf(stderr, "Speed: %.3f bytes/sec during %.3f seconds\n",
                  speed_upload, total_time);

        }
        /* always cleanup */ 
        curl_easy_cleanup(curl);
    }
    free(dest);
    fclose(fd);
    return 0;
}

// Code framework from https://gist.github.com/clemensg/4960504
int asynch_send(char *local_fn, char *remote_path, char *host, long port) {
   	CURLM *cm=NULL;
    CURL *eh=NULL;
    CURLMsg *msg=NULL;
    CURLcode return_code=0;
    int still_running=0, i=0, msgs_left=0;
    int http_status_code;
    const char *szUrl;
    char *dest;

    curl_global_init(CURL_GLOBAL_ALL);

    // Initialize multi, single
    cm = curl_multi_init();
    eh = curl_easy_init();
    //init_single(eh, local_fn, remote_path, host, port, dest);
    curl_multi_add_handle(cm, eh); 
    curl_multi_perform(cm, &still_running);

    do {
        int numfds=0;
        int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
        if(res != CURLM_OK) {
            fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
            return EXIT_FAILURE;
        }
        curl_multi_perform(cm, &still_running);

    } while(still_running);

    while ((msg = curl_multi_info_read(cm, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            eh = msg->easy_handle;

            return_code = msg->data.result;
            if(return_code!=CURLE_OK) {
                fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                continue;
            }

            // Get HTTP status code
            http_status_code=0;
            szUrl = NULL;

            curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
            curl_easy_getinfo(eh, CURLINFO_PRIVATE, &szUrl);

            if(http_status_code==200) {
                printf("200 OK for %s\n", szUrl);
            } else {
                fprintf(stderr, "GET of %s returned http status code %d\n", szUrl, http_status_code);
            }

            curl_multi_remove_handle(cm, eh);
            curl_easy_cleanup(eh);
        }
        else {
            fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
        }
    }

    curl_multi_cleanup(cm);
    free(dest);
    return EXIT_SUCCESS; 
}

// curl created with curl_easy_init()
void init_single(CURL *curl, FILE *local_fd, long port, char *dest, curl_off_t f_size) { 
    /* upload to this place */ 
    curl_easy_setopt(curl, CURLOPT_PORT, port);
    curl_easy_setopt(curl, CURLOPT_URL, dest);
    /* tell it to "upload" to the URL */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    /* set where to read from */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, local_fd);

    /* and give the size of the upload (optional) */ 
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, f_size);

    /* enable verbose for easier tracing */ 
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

void create_full_path(char *remote_path, char *host, char **dest) {
    // Construct full destination
    *dest = (char *)malloc(strlen(host) + strlen(remote_path) + 1);
    strcpy(*dest, host);
    strcat(*dest, remote_path);
}
