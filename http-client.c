#include "http-client.h"

int main(void) {
    return blocking_send("test.bin", "output.bin", "http://localhost/", 8888);
    //return asynch_send("test.bin", "output.bin", "http://localhost/", 8888);
}

// Can possibly move curl init code out, allow for reusing connection
int blocking_send(char *local_fn, char *remote_path, char *host, long port) {
    CURL *curl;
    CURLcode res;
    struct stat file_info;
    double speed_upload, total_time;
	struct progress prog;

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
        prog.lastruntime = 0;
        prog.curl = curl;
        init_single(curl, fd, port, dest, (curl_off_t)file_info.st_size, &prog);
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
    CURL *curl;
    CURLMsg *msg=NULL;
    CURLcode res;
    int still_running=0, msgs_left=0;
    struct stat file_info;
    char *dest;
    double speed_upload, total_time;
	
	struct progress prog;
    FILE *fd;
    fd = fopen(local_fn, "rb"); /* open file to upload */ 
    if(!fd)
        return 1; /* can't continue */ 

    /* to get the file size */ 
    if(fstat(fileno(fd), &file_info) != 0)
        return 1; /* can't continue */ 
    
    curl_global_init(CURL_GLOBAL_ALL);

    // Initialize multi, single
    cm = curl_multi_init();
    curl = curl_easy_init();
    if (curl == NULL)
        exit(1);

    prog.lastruntime = 0;
    prog.curl = curl;

    create_full_path(remote_path, host, &dest);
    init_single(curl, fd, port, dest, (curl_off_t)file_info.st_size, &prog);
    curl_multi_add_handle(cm, curl); 
    curl_multi_perform(cm, &still_running);

    // Perform request
    // Execute other code inside body
    do {
        int numfds=0;
        int res_code = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
        if(res_code != CURLM_OK) {
            fprintf(stderr, "error: curl_multi_wait() returned %d\n", res_code);
            return EXIT_FAILURE;
        }
        curl_multi_perform(cm, &still_running);

    } while(still_running);
    while ((msg = curl_multi_info_read(cm, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            curl = msg->easy_handle;

            res = msg->data.result;
            if(res != CURLE_OK) {
                fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                continue;
            }
            else {
                /* now extract transfer info */ 
                curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
                curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

                fprintf(stderr, "Speed: %.3f bytes/sec during %.3f seconds\n",
                      speed_upload, total_time);
            }
            curl_multi_remove_handle(cm, curl);
            curl_easy_cleanup(curl);
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
void init_single(CURL *curl, FILE *local_fd, long port, char *dest, curl_off_t f_size, struct progress *prog) { 
    /* upload to this place */ 
    curl_easy_setopt(curl, CURLOPT_PORT, port);
    curl_easy_setopt(curl, CURLOPT_URL, dest);
    /* tell it to "upload" to the URL */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    // Progress callback options
 	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, info_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, prog);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    /* set where to read from */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, local_fd);

    /* and give the size of the upload (optional) */ 
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, f_size);

    /* enable verbose for easier tracing */ 
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

// Callback from progressfunc.c example
int info_callback(void *p, curl_off_t dltotal, curl_off_t dlnow,
                  curl_off_t ultotal, curl_off_t ulnow) {
	// Recast struct to progress
	struct progress *myp = (struct progress *)p;
	CURL *curl = myp->curl;
	double curtime = 0;

	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);

	/* under certain circumstances it may be desirable for certain functionality
	 to only run every N seconds, in order to do this the transaction time can
	 be used */ 
	if ((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL) {
		myp->lastruntime = curtime;
		fprintf(stderr, "TOTAL TIME: %f \r\n", curtime);
	}

	fprintf(stderr, "UP: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
		  "  DOWN: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
		  "\r\n",
		  ulnow, ultotal, dlnow, dltotal);
	return 0;
}

void create_full_path(char *remote_path, char *host, char **dest) {
    // Construct full destination
    *dest = (char *)malloc(strlen(host) + strlen(remote_path) + 1);
    strcpy(*dest, host);
    strcat(*dest, remote_path);
}

