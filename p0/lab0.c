/*
NAME: Jianzhi Liu
EMAIL: ljzprivate@yahoo.com
ID: 204742214
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

const int DEBUG = 0;

// make filename the new fd0
void redirect_input(char *filename) {
  if (DEBUG) printf("DEBUG: Redirecting stdin to %s\n",filename);
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Error:\n  Arg: --input\n  File: %s\n  %s\n", filename, strerror(errno));
    exit(2);
  }
  else {
    close(0);
    dup(fd);
    close(fd);
    if (DEBUG) printf("DEBUG: Input redirected.\n");
  }
}

// make filename the new fd1
void redirect_output(char *filename) {
  if (DEBUG) printf("DEBUG: Redirecting stdout to %s\n",filename);
  int fd = creat(filename, 0666);
  if (fd == -1) {
    fprintf(stderr, "Error:\n  Arg: --output\n  File: %s\n  %s\n", filename, strerror(errno));
    exit(3);
  }
  else {
    close(1);
    dup(fd);
    close(fd);
    if (DEBUG) printf("DEBUG: Output redirected.\n");
  }
}

// cause segmentation fault by null pointer reference
void trig_segfault () {
  if (DEBUG) printf("DEBUG: Triggering segfault\n");
  char *trigger = NULL;
  *trigger = 0;
}

void sig_handler(int sig) {
  if (sig == SIGSEGV) {
    fprintf(stderr, "SEGV signal caught.\n");
    exit(4);
  }
}

// copy at most count number of bytes from fd0 to fd1, using the specified buffer
void stdcopy(char *buffer, int count) {
  if (DEBUG) printf("DEBUG: Read from stdin\n");
  int rv1 = read(0, buffer, count);
  int rv2 = 0;
  if (rv1 == -1) {
    fprintf(stderr, "Error:\n  Fail to read from fd0\n  %s\n", strerror(errno));
    exit(5);
  }
  if (DEBUG) printf("DEBUG: Write to stdout\n");
  rv2 = write(1, buffer, rv1);
  if (rv2 == -1) {
    fprintf(stderr, "Error:\n  Fail to write to fd1\n  %s\n", strerror(errno));
    exit(6);
  }
}


int main(int argc, char *argv[]) {
  char *usage = "lab0 [--input=filename] [--output=filename] [--segfault] [--catch]";
  int rv;
  // stipulate four options
  static struct option opts[] = {
    {"input",    1, 0, 'I'},
    {"output",   1, 0, 'O'},
    {"segfault", 0, 0, 'S'},
    {"catch",    0, 0, 'C'},
    {0, 0, 0, 0}
  };
  char opts_parsed[] = {0, 0, 0, 0}; // an array to record which options have been parsed

  char *input_fname = NULL;
  char *output_fname = NULL;
  int buf_size = 10000;
  char buffer[buf_size];

  // 0. process arguments
  opterr = 0;
  while (1) {
    int opt_index = -1;
    rv = getopt_long(argc, argv, "", opts, &opt_index);
    if (rv == -1) break; // parsing finished
    switch (rv) {
      case '?': // unrecognized argument
	fprintf(stderr, "Argument unrecognized.\nUsage: %s\n",usage);
	exit(1);
	break;
      case 'I': // input
	input_fname = optarg;
	if (DEBUG) printf("DEBUG: %c %d %s\n",rv,opt_index,input_fname);
	break;
      case 'O': // output
	output_fname = optarg;
	if (DEBUG) printf("DEBUG: %c %d %s\n",rv,opt_index,output_fname);
        break;
      case 'S':
      case 'C':
	if (DEBUG) printf("DEBUG: %c %d\n",rv,opt_index);
	break;
      default:
	if (DEBUG) printf("DEBUG: Unexpected error with parsing\n");
    }
    // record & check repetitive options
    if (!opts_parsed[opt_index])
      opts_parsed[opt_index]++;
    else {
      fprintf(stderr, "Repetitive option.\nUsage: %s\n", usage);
      exit(1);
    }
  }
  // 1. redirections
  if (opts_parsed[0]) redirect_input(input_fname);
  if (opts_parsed[1]) redirect_output(output_fname);
  // 2. register signal handler
  if (opts_parsed[3]) {
    if (DEBUG) printf("DEBUG: Register SEGV handler\n");
    signal(SIGSEGV, sig_handler);
  }
  // 3. cause the segfault
  if (opts_parsed[2]) trig_segfault();
  // 4. copy stdin to stdout
  stdcopy(buffer, buf_size);
  
  return 0;
}
