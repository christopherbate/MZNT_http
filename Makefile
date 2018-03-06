CC=gcc # Setting compiler
LDLIBS= -lcurl
CFLAGS= -Wall -Wextra # Specified by writeup
HEADERS= http-client.h

# Ensures make normally builds, doesn't clean unless specified
default: http-client

multi-lookup: http-client.c $(HEADERS)
	$(CC) $(CFLAGS) http-client.c $(HEADERS) -o http-client $(LDFLAGS)

clean:
	rm -rf http-client

