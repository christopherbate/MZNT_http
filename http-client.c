#include "http-client.h"

void set_global_opts(CURL *curl, long port);
int info_callback(void *p, curl_off_t dltotal, curl_off_t dlnow,
                  curl_off_t ultotal, curl_off_t ulnow);
void create_full_path(char *remote_path);
void init_file_upload(char *local_fn, char *remote_path);

int main(void) {
    //return blocking_send("test.bin", "output.bin", "http://localhost/", 8888);
    curl_init("http://localhost/", 8888);
    return asynch_send("test.bin", "testdir/output.bin");
}

void curl_init(char *host, long port) {
    // Global curl initialization
    // Not thread safe, must be called once
    curl_global_init(CURL_GLOBAL_ALL);

    // Initialize multi, single
    cm = curl_multi_init();
    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "Curl easy init failed\n");
        exit(1);
    }

    curl_progress.lastruntime = 0;
    curl_progress.curl = curl;

    remote_base_url = host;
    full_remote_path = malloc(1);
   
    // Initialize global curl easy struct
    set_global_opts(curl, port);
    // Add single handle to multi
    curl_multi_add_handle(cm, curl); 
}

void curl_destroy() {
    curl_multi_remove_handle(cm, curl);
    curl_easy_cleanup(curl);
    curl_multi_cleanup(cm);

    free(full_remote_path);
}

// Code framework from https://gist.github.com/clemensg/4960504
int asynch_send(char *local_fn, char *remote_path) {
    CURLMsg *msg=NULL;
    CURLcode res;
    int still_running=0, msgs_left=0;
    double speed_upload, total_time;
	
    init_file_upload(local_fn, remote_path);
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
        }
        else {
            fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
        }
    }

    return EXIT_SUCCESS; 
}

/*
// Can possibly move curl init code out, allow for reusing connection
int blocking_send(char *local_fn, char *remote_path, char *host, long port) {
    CURL *curl;
    CURLcode res;
    struct stat file_info;
    double speed_upload, total_time;
	struct progress prog;

    FILE *fd;
    fd = fopen(local_fn, "rb"); // open file to upload
    if(!fd)
        return 1; // can't continue

    // to get the file size
    if(fstat(fileno(fd), &file_info) != 0)
        return 1; // can't continue 

    curl = curl_easy_init();

    // Construct full destination
    char *dest;
    create_full_path(remote_path, host, &dest);

    if(curl) {
        prog.lastruntime = 0;
        prog.curl = curl;
        init_single(curl, fd, port, dest, (curl_off_t)file_info.st_size, &prog);
        res = curl_easy_perform(curl);
        // Check for errors
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));

        }
        else {
            // now extract transfer info
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

            fprintf(stderr, "Speed: %.3f bytes/sec during %.3f seconds\n",
                  speed_upload, total_time);

        }
        curl_easy_cleanup(curl);
    }
    fclose(fd);
    return 0;
}
*/
// Function for onetime curl struct setup
// Doesn't include file stuff
void set_global_opts(CURL *curl, long port) { 
    /* upload to this place */ 
    curl_easy_setopt(curl, CURLOPT_PORT, port);
    /* tell it to "upload" to the URL */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    // Progress callback options
 	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, info_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &curl_progress);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    /* enable verbose for easier tracing */ 
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

void init_file_upload(char *local_fn, char *remote_path) {
    struct stat file_info;
    create_full_path(remote_path);
    
    FILE *fd;
    fd = fopen(local_fn, "rb"); /* open file to upload */ 
    if(!fd)
        exit(1); /* can't continue */ 
    /* to get the file size */ 
    if(fstat(fileno(fd), &file_info) != 0)
        exit(1); /* can't continue */ 
    
    curl_easy_setopt(curl, CURLOPT_URL, full_remote_path);
    /* set where to read from */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, fd);
    /* and give the size of the upload (optional) */ 
   curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, file_info.st_size);
}

void create_full_path(char *remote_path) {
    char* tmp = (char *)realloc(full_remote_path, strlen(remote_base_url) + strlen(remote_path) + 1);
    // Construct full destination using global base url
    full_remote_path = strcpy(tmp, remote_base_url);
    strcat(full_remote_path, remote_path);
    tmp = NULL;
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
