#include "http-client.h"

int main(void) {
    curl_init("http://localhost/", 8888);
    asynch_send("test.bin", "testdir/output.bin");
    sleep(3);
    asynch_send("test.bin", "anotherdir/bn.bin");
    return curl_destroy();
}
