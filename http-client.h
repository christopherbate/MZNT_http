#ifndef http_client_h
#define http_client_h

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

int main(int argc, char **argv);
int blocking_send(char *local_fn, char *remote_path, char *host, long port);

#endif /* http_client_h */
