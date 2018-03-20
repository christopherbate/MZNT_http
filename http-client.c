#include "http-client.h"

int set_global_opts(CURL *curl, long port);
int info_callback(void *p, curl_off_t dltotal, curl_off_t dlnow,
                  curl_off_t ultotal, curl_off_t ulnow);
int create_full_path(char *remote_path);
int init_file_upload(CURL *curl, char *local_fn, char *remote_path);

int curl_init(char *host, long port) {
    remote_base_url = host;
    remote_port = port;
    // Realloc treats null pointer as normal malloc
    full_remote_path = NULL;
    
    // Global curl initialization
    // Not thread safe, must be called once
    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "Curl easy init failed\n");
        return -1;
    }
    if (set_global_opts(curl, remote_port) < 0)
        return -1; 
    
    // Initialize multi, single
    cm = curl_multi_init();
    if (cm == NULL) {
        fprintf(stderr, "Curl multi init failed\n");
        return -1;
    }

    /*
    curl_progress.lastruntime = 0;
    curl_progress.curl = curl;
    */
   
    // File lock declarations
    pthread_mutex_init(&send_lock, NULL);
    // Thread flag initialization
    in_progress = 0;
    cancel_flag = 0;

    return 0;
}

int curl_destroy() {
    curl_easy_cleanup(curl);
    curl_multi_cleanup(cm);

    free(full_remote_path);

    return 0;
}

int asynch_send(char *local_fn, char *remote_path) {
    // Check to make sure more than one simultaneous transfer doesn't occur
    pthread_mutex_lock(&send_lock);
    printf("%d\n", in_progress);
    if (in_progress) {
        fprintf(stderr, "error: attempted to start second transfer\n");
        return -1;
    }
    in_progress = 1;
    pthread_mutex_unlock(&send_lock);
    
    pthread_t id;

    // Set thread to detached
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // Create, init struct
    struct req_info *tmp = malloc(sizeof(struct req_info));
    tmp->local_fn = local_fn;
    tmp->remote_path = remote_path;

    // Create thread
    pthread_create(&id, &attr, send_worker, (void *)tmp);  
    return 0; 
}

void *send_worker(void *arg) {
    struct req_info *ri = (struct req_info*)arg;


    sleep(3);
    CURLMsg *msg=NULL;
    CURLcode res;
    int still_running=0, msgs_left=0;
    double speed_upload, total_time;
    
    //CURL *curl;
    // Initialize global curl easy struct
    if (init_file_upload(curl, ri->local_fn, ri->remote_path) < 0)
        return (void*)-1;

    //int i = curl_easy_perform(curl);
    //fprintf(stderr, "error: curl_multi_wait() returned %s\n", curl_easy_strerror(i));
    //curl_easy_cleanup(curl);
    
    // Add single handle to multi
    curl_multi_add_handle(cm, curl); 
    curl_multi_perform(cm, &still_running);

    // Perform request
    // Execute other code inside body
    do {
        int numfds=0;
        int res_code = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
        //printf("%d\n", res_code); 
        pthread_mutex_lock(&send_lock);
        if(res_code != CURLM_OK && cancel_flag != 1) {
            pthread_mutex_unlock(&send_lock);
            fprintf(stderr, "error: curl_multi_wait() returned %s\n", curl_multi_strerror(res_code));
            
            // Removing handle mid-transfer will abort request
            curl_multi_remove_handle(cm, curl);
            
            // Cleanup
            fclose(curr_fd);
            pthread_mutex_lock(&send_lock);
            in_progress = 0;
            pthread_mutex_unlock(&send_lock);
            free(ri);
            
            return (void*)-1;
        }
        pthread_mutex_unlock(&send_lock);
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
                // now extract transfer info
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
    curl_multi_remove_handle(cm, curl);
    
    fclose(curr_fd);
    
    pthread_mutex_lock(&send_lock);
    in_progress = 0;
    pthread_mutex_unlock(&send_lock);

    printf("thread done\n");

    free(ri);
    pthread_exit(NULL);
}

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
