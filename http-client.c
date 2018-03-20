#include "http-client.h"

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

    CURLMsg *msg=NULL;
    CURLcode res;
    int still_running=0, msgs_left=0;
    double speed_upload, total_time;
    
    // Initialize global curl easy struct
    if (init_file_upload(curl, ri->local_fn, ri->remote_path) < 0)
        return (void*)-1;

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
            cancel_flag = 0;
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
    cancel_flag = 0;
    pthread_mutex_unlock(&send_lock);

    printf("thread done\n");

    free(ri);
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

int cancel_send() {
    pthread_mutex_lock(&send_lock);
    if (in_progress)
        cancel_flag = 1;
    pthread_mutex_unlock(&send_lock);
    return 0;
}
