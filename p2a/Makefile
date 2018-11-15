# NAME: Jianzhi Liu
# EMAIL: ljzprivate@yahoo.com
# ID: 204742214

CC = gcc
CFLAGS = -Wall -Wextra -lpthread
ID = 204742214
FILES = lab2_add.c SortedList.h SortedList.c lab2_list.c Makefile lab2_*.csv *.png lab2_*.gp test_add.sh test_list.sh README

build: lab2_add.c lab2_list.c SortedList.c SortedList.h
	$(CC) $(CFLAGS) -o lab2_add lab2_add.c;\
	$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c
add: lab2_add.c
	$(CC) $(CFLAGS) -o lab2_add lab2_add.c
list: lab2_list.c SortedList.c SortedList.h
	$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c

tests:
	@chmod +x test_add.sh test_list.sh;\
	./test_add.sh all; ./test_list.sh all

graphs:
	@chmod +x lab2_add.gp lab2_list.gp;\
	./lab2_add.gp; ./lab2_list.gp

dist: build tests graphs
	@tar -cvzf lab2a-$(ID).tar.gz $(FILES)

clean:
	rm -f lab2a-$(ID).tar.gz lab2_add lab2_list
