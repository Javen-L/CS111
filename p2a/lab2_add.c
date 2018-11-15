/*
NAME: Jianzhi Liu
EMAIL: ljzprivate@yahoo.com
ID: 204742214
*/
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

char DEBUG = 0;

long long count = 0;
int nthreads = 1;
int niters = 1;
int opt_csv = 0;
int opt_yield = 0;
int opt_sync = 0;
char sync_type = 0;
pthread_mutex_t mutex =  PTHREAD_MUTEX_INITIALIZER;
static int slock = 0;

long long nsec_time(struct timespec ts) {
  return ts.tv_sec*1000000000 + ts.tv_nsec;
}

void add(long long *pointer, long long value) {
  if (!opt_sync) {
    long long sum = *pointer + value;
    if (opt_yield) sched_yield();
    *pointer = sum;
  }
  else if (sync_type == 'm') {
    pthread_mutex_lock(&mutex);
    long long sum = *pointer + value;
    if (opt_yield) sched_yield();
    *pointer = sum;
    pthread_mutex_unlock(&mutex);
  }
  else if (sync_type == 's') {
    while(__sync_lock_test_and_set(&slock,1))
      ; // spin
    long long sum = *pointer + value;
    if (opt_yield) sched_yield();
    *pointer = sum;
    __sync_lock_release(&slock);
  }
  else if (sync_type == 'c') {
    long long oldval, sum;
    do {
      oldval = *pointer;
      sum = oldval + value;
      if (opt_yield) sched_yield();
    }
    while(__sync_val_compare_and_swap(pointer, oldval, sum) != oldval);
  }
}

void *thread_func() {
  int i;
  for (i = 0; i < niters; i++) {
    add(&count, 1);
  }
  for (i = 0; i < niters; i++) {
    add(&count, -1);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  // parsing
  char *usage = "lab2_add [--threads=# of threads] [--iterations=# of iterations] [--yield] [--sync=m|s|c]";
  static struct option opts[] = {
    {"threads",    1, 0, 't'},
    {"iterations", 1, 0, 'i'},
    {"yield",      0, 0, 'y'},
    {"sync",       1, 0, 's'},
    {"csv",       0, 0, 'c'},
    {"debug",      0, 0, 'd'},
    {0, 0, 0, 0}
  };
  while (1) {
    int opt_index = -1;
    int rv = getopt_long(argc, argv, "", opts, &opt_index);
    if (rv == -1) break;
    switch (rv) {
    case '?':
      fprintf(stderr, "Argument unrecognized.\nUsage: %s\n",usage);
      exit(1);
      break;
    case 't':
      nthreads = atoi(optarg);
      if (DEBUG) printf("[DEBUG] # of threads set to %d\n", nthreads);
      break;
    case 'i':
      niters = atoi(optarg);
      if (DEBUG) printf("[DEBUG] # of iterations set to %d\n", niters);
      break;
    case 'y':
      opt_yield = 1;
      if (DEBUG) printf("[DEBUG] Option yield set\n");
      break;
    case 's':
      opt_sync = 1;
      sync_type = optarg[0];
      if (!(sync_type == 'm' || sync_type == 's' || sync_type == 'c')) {
	fprintf(stderr, "--sync=%c is not supported\n", sync_type);
	exit(1);
      }
      if (DEBUG) printf("[DEBUG] Option sync set to %c\n", sync_type);
      break;
    case 'c':
      opt_csv = 1;
      if (DEBUG) printf("[DEBUG] Option csv set\n");
      break;
    case 'd':
      DEBUG = 1;
      break;
    default:
      fprintf(stderr, "Unexpected error in parsing.\nReturn value: %c\n",rv);
      exit(1);
    }
  }
  // record start time
  struct timespec ts_start;
  if (clock_gettime(CLOCK_MONOTONIC, &ts_start) == -1) {
    fprintf(stderr, "Failed to get time from clock\n");
    exit(2);
  }
  // set up threads
  pthread_t *threads = (pthread_t *) malloc(nthreads*sizeof(pthread_t));
  if (!threads) {
    fprintf(stderr, "Failed to malloc for threads\n");
    exit(2);
  }
  int i;
  for (i = 0; i < nthreads; i++) {
    if (pthread_create(&threads[i], NULL, thread_func, NULL)) {
      fprintf(stderr, "Failed to create threads: %s\n", strerror(errno));
      free(threads);
      exit(2);
    }
  } 
  // wait for threads
  for (i = 0; i < nthreads; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "Failed to join threads: %s\n", strerror(errno));
      free(threads);
      exit(2);
    }
  }
  free(threads);
  // record end time
  struct timespec ts_end;
  if (clock_gettime(CLOCK_MONOTONIC, &ts_end) == -1) {
    fprintf(stderr, "Failed to get time from clock\n");
    exit(2);
  }
  // prepare output
  char* ytag;
  if (opt_yield) ytag = "-yield";
  else ytag = "\0";
  char *test_name = malloc(128*sizeof(char));
  if (opt_sync)
    sprintf(test_name, "add%s-%c", ytag, sync_type);
  else
    sprintf(test_name, "add%s-none", ytag);
  int nops = nthreads * niters * 2;
  long long tot_time = nsec_time(ts_end) - nsec_time(ts_start);
  int time_per_op = tot_time / nops;
  char csv_output[256];
  sprintf(csv_output, "%s,%d,%d,%d,%lld,%d,%lld\n",
	  test_name, nthreads, niters, nops, tot_time, time_per_op, count);
  free(test_name);
  if (!opt_csv)
    printf("%s",csv_output);
  if (opt_csv) {
    int fd = open("lab2_add.csv", O_RDWR|O_APPEND|O_CREAT, 0666);
    if (fd == -1) {
      fprintf(stderr, "Failed to open csv file: %s\n", strerror(errno));
      exit(2);
    }
    if (write(fd, csv_output, strlen(csv_output)) == -1) {
      fprintf(stderr, "Failed to write to csv file: %s\n", strerror(errno));
      exit(2);
    }
  }
  return 0;
}
