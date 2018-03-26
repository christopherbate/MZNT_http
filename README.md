# MZNT_http
HTTP client, server code for MZNT logger.

## Overview
This repo is separated into two different chunks: http server and client code for the MZNT logging system.

### Server
The server is a very basic python http server using `HTTPServer`. It has skeleton functions to handle GET and POST requests while the main request handling code is in the `do_PUT` function. This function simply takes the relative path specified in the PUT request, creates directories as needed, and writes the raw bytes to the destination file. 

The server responds to put requests with "HTTP/1.1" protocol version, response code of 200, and a "Content-Length" of 0 to conform to HTTP protocol specifications for persistent connections.

In the future, this PUT handler code may include logic to keep track of "pre"/"post" file pairs and spawn subprocesses to perform data unpacking and analysis steps.

### Client
The `http-client` module provides asynchronous HTTP PUT request functionality for the MicroZed MZNT logger code through the libcurl library. While this code is asynchronous, it is intended to be used with a single transfer at a time.

To initialize the module, call `curl_init(char *host, long port)` with the hostname (e.g. "http://localhost/") and the port number (e.g. 8888) of the destination server. This function performs global libcurl initialization functions as well as other one-time housekeeping.

To perform a file transfer to the server, call `asynch_send(char *filename, char *rem_path)` with the filename (or relative file path) of the file you wish to send as well as the intended destination path (e.g. `testdir/output.bin`). If another transfer is in progress, this function will return -1. Otherwise, it will spawn a new detached thread to handle the transfer and return 0. Note that the calling process must remain alive for the entire duration of the transfer.

The `send_worker` function will perform some option-setting for libcurl and perform the transfer, printing information to console upon successful transfer or error.

To retrieve progress information on the current transfer, call `status_send()` to return the number of bytes currently uploaded.

To cancel a transfer in progress, call `cancel_send()`. This function is not a hard abort and will have no affect if the transfer is complete but housekeeping in `send_worker()` hasn't finished.

To deallocate everything safely once done logging, call `curl_destroy()`.

## How to run

### Dependencies
To build, libcurl and pthreads must be available on the system.

### Build
Run `make clean; make` in the source directory to build. This repo contains a driver file `http-driver.c` for testing purposes.

### Example
```C
#include "http-client.h"

int main(void) {
    curl_init("http://localhost/", 1337);
    asynch_send("test.bin", 0, "testdir/output.bin");
    sleep(5);
    asynch_send("test.bin", 10000000, "anotherdir/bn.bin");
    sleep(5);
    asynch_send("test.bin", 40000000, "anotherdir/bn.bin");
    sleep(5);
    return curl_destroy();
}
```
