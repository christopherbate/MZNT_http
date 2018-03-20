#include "http-client.h"

int main(void) {
    curl_init("http://localhost/", 8888);
    asynch_send("test.bin", "testdir/output.bin");
    printf("done\n");
    
    sleep(10);

    asynch_send("test.bin", "anotherdir/bn.bin");
    
    sleep(10);
    return curl_destroy();
}
