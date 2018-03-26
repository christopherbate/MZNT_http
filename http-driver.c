#include "http-client.h"

int main(void) {
    curl_init("http://localhost/", 1337);
    asynch_send("test.bin", 0, "testdir/output.bin");
    /*
    unsigned long s;
    while (1) {
        s = (unsigned long)status_send();
        if (s >= 22343001)
            break;
        printf("Uploaded %lu bytes\n", s);
    }
    */
    sleep(5);
    asynch_send("test.bin", 10000000, "anotherdir/bn.bin");
    sleep(5);
    asynch_send("test.bin", 40000000, "anotherdir/bn.bin");
    sleep(5);
    return curl_destroy();
}
