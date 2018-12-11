# NAME: Jianzhi Liu
# EMAIL: ljzprivate@yahoo.com
# ID: 204742214

CC = gcc
CFLAGS = -Wall -Wextra -lmraa -lm
SSLFLAGS = -lssl -lcrypto
HOSTNAME = lever.cs.ucla.edu
ID = 204742214
FILES = lab4c_tcp.c lab4c_tls.c Makefile README

LOG = log.txt
PORT = 18000
SSLPORT = 19000

build: lab4c_tcp.c lab4c_tls.c
	$(CC) $(CFLAGS) -o lab4c_tcp lab4c_tcp.c;
	$(CC) $(CFLAGS) $(SSLFLAGS) -o lab4c_tls lab4c_tls.c
dist:
	tar -cvzf lab4c-$(ID).tar.gz $(FILES)
clean:
	rm -f lab4c-$(ID).tar.gz lab4c_tcp lab4c_tls

ttcp: build
	rm -f ${LOG}
	./lab4c_tcp --d --p=3.14 --s=C --i=${ID} --log=${LOG} --host=${HOSTNAME} ${PORT}
ttls: build
	rm -f ${LOG}
	./lab4c_tls --d --p=3.14 --s=C --i=${ID} --log=${LOG} --host=${HOSTNAME} ${SSLPORT}
