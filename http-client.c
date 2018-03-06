#include "http-client.h"

int main(int argc, char **argv) {
    return blocking_send("test.bin", "output.bin", "http://localhost/", 8888);
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
  char *dest = malloc(strlen(host) + strlen(remote_path) + 1);
  strcpy(dest, host);
  strcat(dest, remote_path);
  
  if(curl) {
    /* upload to this place */ 
    curl_easy_setopt(curl, CURLOPT_PORT, port);
    curl_easy_setopt(curl, CURLOPT_URL, dest);
    /* tell it to "upload" to the URL */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
    
    /* set where to read from */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, fd);
 
    /* and give the size of the upload (optional) */ 
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)file_info.st_size);
 
    /* enable verbose for easier tracing */ 
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

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
