# NAME: Jianzhi Liu
# EMAIL: ljzprivate@yahoo.com
# ID: 204742214

CC = gcc
CFLAGS = -Wall -Wextra
ID = 204742214
INFNAME = a_really_random_input_filename.txt
OUTFNAME = a_really_random_output_filename.txt
DNENAME = a_file_that_really_should_not_exist
TESTMSG = "TEST MESSAGE"

default:
	$(CC) $(CFLAGS) -o lab0 lab0.c

check: default
	@echo $(TESTMSG) > $(INFNAME)
        # 0. no option
	@./lab0 < $(INFNAME) > $(OUTFNAME); \
	if [ $$? -ne 0 ]; \
	then echo "Test case 0 failed."; exit 1; fi
	@cmp -s $(INFNAME) $(OUTFNAME) || ( echo "Test case 0 failed."; exit 1 )
        # 00. --input(normal) --output(normal)
	@./lab0 --input=$(INFNAME) --output=$(OUTFNAME); \
	if [ $$? -ne 0 ]; \
	then echo "Test case 00 failed."; exit 1; fi
	@cmp -s $(INFNAME) $(OUTFNAME) || ( echo "Test case 00 failed."; exit 1 )
        # 1. unrecognized argument
	@./lab0 --unrecgArg 2> /dev/null; \
	if [ $$? -ne 1 ]; \
	then echo "Test case 1 failed."; exit 1; fi
        # 2. --input(error)
	@./lab0 --input=$(DNENAME) 2> /dev/null; \
	if [ $$? -ne 2 ]; \
	then echo "Test case 2 failed."; exit 1; fi
        # 3. --output(error)
	@chmod -w $(OUTFNAME)
	@./lab0 --output=$(OUTFNAME) 2> /dev/null; \
	if [ $$? -ne 3 ]; \
	then echo "Test case 3 failed."; exit 1; fi
	@chmod +w $(OUTFNAME)
        # 4. --segfault --catch
	@./lab0 --segfault --catch 2> /dev/null; \
	if [ $$? -ne 4 ]; \
	then echo "Test case 4 failed."; exit 1; fi
        # remove side effects
	@rm -f $(INFNAME) $(OUTFNAME)
	@echo "Smoke test passed."

clean:
	rm -f lab0 lab0-$(ID).tar.gz

dist:
	tar -czf lab0-$(ID).tar.gz lab0.c Makefile README backtrace.png breakpoint.png
