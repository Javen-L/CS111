# NAME: Jianzhi Liu
# EMAIL: ljzprivate@yahoo.com
# ID: 204742214

CC = gcc
CFLAGS = -Wall -Wextra -lpthread -g
ID = 204742214
FILES = SortedList.h SortedList.c lab2_list.c Makefile lab2b_list.csv profile.out *.png plot.gp test.sh README

build: lab2_list.c SortedList.c SortedList.h
	$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c

tests:
	@chmod +x test.sh; ./test.sh all

profile: build
	rm -f lab2_list.prof profile.out
	LD_PRELOAD=\
	"/u/eng/class/classjys/cs111/p2b/gperftools_install/lib/libprofiler.so" \
	CPUPROFILE=lab2_list.prof ./lab2_list --iterations=1000 --threads=12 --sync=s
	pprof --text ./lab2_list lab2_list.prof > profile.out
	pprof --list=thread_func ./lab2_list ./lab2_list.prof >> profile.out
	rm lab2_list.prof

graphs:
	@chmod +x plot.gp; ./plot.gp

dist: tests profile graphs
	@tar -cvzf lab2b-$(ID).tar.gz $(FILES)

clean:
	rm -f lab2b-$(ID).tar.gz lab2_add lab2_list
