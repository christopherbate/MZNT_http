#include "http-client.h"

static CURLM *cm;
static CURL *curl;

static FILE *curr_fd;

static char *local_fn;
static char *remote_path;

static int remote_port;
static char *remote_base_url;
static char *full_remote_path;

static int in_progress;
static int cancel_flag;
static pthread_mutex_t send_lock;

void *send_worker();
int set_global_opts();
int init_file_upload();
int create_full_path();

int asynch_send(char *filename, char *rem_path) {
    // Check to make sure more than one simultaneous transfer doesn't occur
    pthread_mutex_lock(&send_lock);
    printf("%d\n", in_progress);
    if (in_progress) {
        fprintf(stderr, "error: attempted to start second transfer\n");
        return -1;
    }
    in_progress = 1;
    pthread_mutex_unlock(&send_lock);
    
    local_fn = filename;
    remote_path = rem_path;

    pthread_t id;
    // Set thread to detached
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // Create thread
    pthread_create(&id, &attr, send_worker, NULL);  
    return 0; 
}

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
void *send_worker() {
    CURLMsg *msg=NULL;
    CURLcode res;
    int still_running=0, msgs_left=0;
    double speed_upload, total_time;
    
    // Initialize global curl easy struct
    if (init_file_upload() < 0)
        return (void*)-1;

    // Add single handle to multi
    curl_multi_add_handle(cm, curl); 
    curl_multi_perform(cm, &still_running);

    // Perform request
    // Execute other code inside body
    do {
        int numfds=0;
        int res_code = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
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
            cancel_flag = 0;
            pthread_mutex_unlock(&send_lock);
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
    cancel_flag = 0;
    pthread_mutex_unlock(&send_lock);

    printf("thread done\n");

    pthread_exit(NULL);
}

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
    if (set_global_opts() < 0)
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
    pthread_mutex_lock(&send_lock);
    if (in_progress) {
        fprintf(stderr, "error: Cannot destroy, transfer in progress\n");
        return -1;
    }
    pthread_mutex_unlock(&send_lock);
    
    curl_easy_cleanup(curl);
    curl_multi_cleanup(cm);

    free(full_remote_path);

    return 0;
}

int cancel_send() {
    pthread_mutex_lock(&send_lock);
    if (in_progress)
        cancel_flag = 1;
    pthread_mutex_unlock(&send_lock);
    return 0;
}

/*
    REQUIRES:
        Valid (initialized) pointer to curl struct
        port number of server

    MODIFIES:
        curl

    RETURNS:
        0 on success
*/
int set_global_opts() { 
    /* upload to this place */ 
    curl_easy_setopt(curl, CURLOPT_PORT, remote_port);
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

/*
    REQUIRES:
        Valid (initialized) pointer to curl struct
        curl struct pointer, local_fn, remote_path, full_remote_path

    MODIFIES:
        curr_fd, curl struct, full_remote_path (through create_full_path)

    RETURNS:
        0 on success
        -1 on bad file open, fstat call
*/
int init_file_upload() {
    struct stat file_info;
    create_full_path();
    
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

/*
    REQUIRES:
        successful completion of curl_init (for remote base url)
        remote_base_url, remote_path, full_remote_path

    MODIFIES:
        full_remote_path

    RETURNS:
        0 on success
        -1 for bad realloc
*/
int create_full_path() {
    char* tmp = (char *)realloc(full_remote_path, strlen(remote_base_url) + strlen(remote_path) + 1);

    if (tmp == NULL)
        return -1;
    // Construct full destination using global base url
    full_remote_path = strcpy(tmp, remote_base_url);
    strcat(full_remote_path, remote_path);
    tmp = NULL;

    return 0;
}

