# NAME: Jianzhi Liu, Yinsheng Zhou
# EMAIL: ljzprivate@yahoo.com, jacobzhou@g.ucla.edu
# ID: 204742214, 004817743

CC = gcc
CFLAGS = -Wall -Wextra
ID = 204742214
FILES = ext2_fs.h lab3a.c Makefile README

build: ext2_fs.h lab3a.c
	$(CC) $(CFLAGS) -o lab3a lab3a.c

dist:
	@tar -cvzf lab3a-$(ID).tar.gz $(FILES)

clean:
	rm -f lab3a-$(ID).tar.gz lab3a

#check: dist
#	cp lab3a-$(ID).tar.gz check/; \
	cd check; \
	./P3A_check.sh 204742214; \
	cd ..
