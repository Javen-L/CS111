# NAME: Jianzhi Liu
# EMAIL: ljzprivate@yahoo.com
# ID: 204742214

CC = gcc
CFLAGS = -Wall -Wextra -lmcrypt
ID = 204742214

default: lab1b-client.c lab1b-server.c
	$(CC) $(CFLAGS) -o lab1b-client lab1b-client.c;\
	$(CC) $(CFLAGS) -o lab1b-server lab1b-server.c
clt:
	$(CC) $(CFLAGS) -o lab1b-client lab1b-client.c
srv:
	$(CC) $(CFLAGS) -o lab1b-server lab1b-server.c
clean:
	rm -f lab1b-client lab1b-server lab1b-$(ID).tar.gz
dist:
	tar -czf lab1b-$(ID).tar.gz lab1b-client.c lab1b-server.c my.key Makefile README
