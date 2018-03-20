#include "http-client.h"

int main(void) {
    curl_init("http://localhost/", 8888);
    asynch_send("test.bin", "testdir/output.bin");
    printf("done\n");
    asynch_send("test.bin", "anotherdir/bn.bin");
    
    sleep(10);

    return curl_destroy();
}
