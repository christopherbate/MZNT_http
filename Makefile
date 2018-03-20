CC=gcc # Setting compiler
LDFLAGS= -lcurl -lpthread
CFLAGS= -Wall -Wextra # Specified by writeup
HEADERS= http-client-helper.h http-client.h 

# Ensures make normally builds, doesn't clean unless specified
default: http-driver

http-client-helper.o: http-client-helper.c $(HEADERS)
	$(CC) -c $(CFLAGS) http-client-helper.c $(HEADERS) $(LDFLAGS)

http-client.o: http-client.c $(HEADERS)
	$(CC) -c $(CFLAGS) http-client.c $(HEADERS) $(LDFLAGS)

http-driver.o: http-driver.c $(HEADERS)
	$(CC) -c $(CFLAGS) http-driver.c $(HEADERS) $(LDFLAGS)

http-driver: http-client-helper.o http-client.o http-driver.o
	$(CC) -o http-driver $(CFLAGS) http-client-helper.o http-client.o http-driver.o $(LDFLAGS)

clean:
	rm -rf http-driver
	rm -rf http-driver.o
	rm -rf http-client.o
	rm -rf http-client-helper.o
	rm -rf http-client.h.gch
	rm -rf http-client-helper.h.gch
