# NAME: Jianzhi Liu
# EMAIL: ljzprivate@yahoo.com
# ID: 204742214

CC = gcc
CFLAGS = -Wall -Wextra -lmraa -lm
ID = 204742214
FILES = lab4b.c Makefile README

build: lab4b.c
	$(CC) $(CFLAGS) -o lab4b lab4b.c

check: build
	echo "OFF\n" > temp.txt; \
	./lab4b < temp.txt; \
	rm temp.txt

dist:
	@tar -cvzf lab4b-$(ID).tar.gz $(FILES)

clean:
	rm -f lab4b-$(ID).tar.gz lab4b
