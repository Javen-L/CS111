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
#include <signal.h>
#include "SortedList.h"

char DEBUG = 0;

SortedList_t *list= NULL;
SortedListElement_t *elements = NULL;
int nelems = 0;

static int nthreads = 1;
static int niters = 1;
int opt_csv = 0;
int opt_yield = 0;
char *yield_str = NULL;
int opt_sync = 0;
char sync_type = 0;
pthread_mutex_t mutex =  PTHREAD_MUTEX_INITIALIZER;
static int slock = 0;

long long nsec_time(struct timespec ts) {
  return ts.tv_sec*1000000000 + ts.tv_nsec;
}

int handle_yield_opt(char *yield_str) {
  int len = strlen(yield_str);
  if (len > 3) return 0;
  char *idl = "idl";
  int idl_counter[3] = {0,0,0};
  int i,j;
  for (i = 0; i < len; i++) {
    for (j = 0; j < 3; j++) {
      if (yield_str[i] == idl[j]) {
	idl_counter[j]++;
	break;
      }
      if (j == 2) return 0;
    }
  }
  for (i = 2; i >= 0; i--) {
    if (idl_counter[i] > 1) return 0;
    opt_yield += idl_counter[i];
    if (i != 0) opt_yield = opt_yield << 1;
  }
  return 1;
}

void malloc_error() {
    fprintf(stderr, "Failed to allocate memory\n");
    exit(2);
}

// generate a random key, return its address
char *rand_key() {
  int max_len = 10;
  int rand_len = rand() % max_len + 1;
  char *key = malloc((rand_len+1)*sizeof(char));
  if (!key) malloc_error();
  int i;
  for (i = 0; i < rand_len; i++) {
    // allow characters with ASCII code from 32 to 127, 96 in total
    key[i] = rand() % 32 + 96;
  }
  key[rand_len] = '\0';
  return key;
}

// free every node in the list, including the string its key points to
void free_list() {
  free(list);
  int i;
  for (i = 0; i < nelems; i++) {
    free((void *) elements[i].key);
  }
  free(elements);
}

void sighandler(int sig) {
  if (sig == SIGSEGV) {
    fprintf(stderr, "Segmentation fault caught\n");
    exit(2);
  }
}

// an auxilary function for debugging
void printls() {
  SortedListElement_t *iter;
  if (!list || list->key) return; // invalid head node
  for (iter = list->next;; iter = iter->next) {
    if (!iter || iter == list) { // empty or a full loop finished
      printf("Head\n");
      break;
    }
    printf("%s\n", iter->key);
  }
  printf("----End\n");
}

void *thread_func(void *p) {
  int offset = (*((int *) p)) * niters;
  int lmt = offset + niters;
  int i;
  if (DEBUG) printf("[DEBUG] Thread %d got elements[%d:%d]\n", *((int *)p), offset, lmt);
  // insert
  for (i = offset; i < lmt; i++) {
    if (!opt_sync) {
      SortedList_insert(list, &elements[i]);
    }
    else if (sync_type == 'm') {
      pthread_mutex_lock(&mutex);
      SortedList_insert(list, &elements[i]);
      pthread_mutex_unlock(&mutex);
    }
    else if (sync_type == 's') {
      while(__sync_lock_test_and_set(&slock,1))
	; // spin
      SortedList_insert(list, &elements[i]);
      __sync_lock_release(&slock);
    }
    //    if (DEBUG) printls();
  }
  // get length
  int len = -1;
  if (!opt_sync) {
    len = SortedList_length(list);
  }
  else if (sync_type == 'm') {
    pthread_mutex_lock(&mutex);
    len = SortedList_length(list);
    pthread_mutex_unlock(&mutex);
  }
  else if (sync_type == 's') {
    while(__sync_lock_test_and_set(&slock,1))
      ; // spin
    len = SortedList_length(list);
    __sync_lock_release(&slock);
  }
  if (len == -1) {
    fprintf(stderr, "Something went wrong with list insert/length\n");
    exit(2);
  }
  // look up & delete
  for (i = offset; i < lmt; i++) {
    if (opt_sync && sync_type == 'm')
      pthread_mutex_lock(&mutex);
    if (opt_sync && sync_type == 's')
      while(__sync_lock_test_and_set(&slock,1))
	; // spin
    SortedListElement_t *p = SortedList_lookup(list, elements[i].key);
    if (!p) {
      fprintf(stderr, "Something went wrong with list insert/lookup\n");
      exit(2);
    }
    int rv = SortedList_delete(p);
    if (rv == 1) {
      fprintf(stderr, "Something went wrong with list insert/lookup/delete\n");
      exit(2);
    }
    if (opt_sync && sync_type == 's')
      __sync_lock_release(&slock);
    if (opt_sync && sync_type == 'm')
      pthread_mutex_unlock(&mutex);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  // parsing
  char *usage = "lab2_list [--threads=# of threads] [--iterations=# of iterations] [--yield=[idl]]";
  static struct option opts[] = {
    {"threads",    1, 0, 't'},
    {"iterations", 1, 0, 'i'},
    {"yield",      1, 0, 'y'},
    {"sync",       1, 0, 's'},
    {"csv",        0, 0, 'c'},
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
      yield_str = optarg;
      if (!handle_yield_opt(optarg)) {
	fprintf(stderr, "yield=%s is not supported\n", optarg);
	exit(1);
      }
      if (DEBUG)
	printf("[DEBUG] Option yield(=%s) set to %d: idl=%d%d%d\n",
		 optarg, opt_yield,
		 !!(opt_yield&INSERT_YIELD),
		 !!(opt_yield&DELETE_YIELD),
		 !!(opt_yield&LOOKUP_YIELD));
      break;
    case 's':
      opt_sync = 1;
      sync_type = optarg[0];
      if (!(sync_type == 'm' || sync_type == 's')) {
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
  // intialize empty list
  list = (SortedList_t*) malloc(sizeof(SortedList_t));
  if (!list) {
    malloc_error();
  }
  list->key = NULL; list->next = NULL; list->prev = NULL;
  if (DEBUG) printf("{DEBUG} Empty list initialized\n");
  // create and initialize threads*iters number of list elements, with random keys
  nelems = nthreads*niters;
  elements = (SortedListElement_t*) malloc(nelems*sizeof(SortedListElement_t));
  if (!elements) {
    free(list);
    malloc_error();
  }
  int i;
  for (i = 0; i < nelems; i++) {
    elements[i].key = rand_key();
    elements[i].prev = NULL;
    elements[i].next = NULL;
  }
  if (DEBUG) printf("{DEBUG} Elements initialized with random keys\n");
  // signal segfault
  signal(SIGSEGV, sighandler);
  // record start time
  struct timespec ts_start;
  if (clock_gettime(CLOCK_MONOTONIC, &ts_start) == -1) {
    fprintf(stderr, "Failed to get time from clock\n");
    free_list();
    exit(2);
  }
  // set up threads
  pthread_t *threads = (pthread_t *) malloc(nthreads*sizeof(pthread_t));
  if (!threads) {
    fprintf(stderr, "Failed to malloc for threads\n");
    exit(2);
  }
  int thread_args[nthreads];
  for (i = 0; i < nthreads; i++) {
    thread_args[i] = i;
  }
  for (i = 0; i < nthreads; i++) {
    if (DEBUG) printf("{DEBUG} thread %d created\n",i);
    if (pthread_create(&threads[i], NULL, thread_func, (void *) &thread_args[i])) {
      fprintf(stderr, "Failed to create threads: %s\n", strerror(errno));
      free(threads);
      free_list();
      exit(2);
    }
  }
  // wait for threads
  for (i = 0; i < nthreads; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "Failed to join threads: %s\n", strerror(errno));
      free(threads);
      free_list();
      exit(2);
    }
  }
  free(threads);
  // record end time
  struct timespec ts_end;
  if (clock_gettime(CLOCK_MONOTONIC, &ts_end) == -1) {
    fprintf(stderr, "Failed to get time from clock\n");
    free_list();
    exit(2);
  }
  int final_len = SortedList_length(list);
  if (final_len) {
    fprintf(stderr, "Final length of the list is none-zero: %d\n", final_len);
    free_list();
    exit(2);
  }
  free_list();
  // prepare output
  char *nonestr = "none";
  if (!opt_yield) yield_str = nonestr;
  char *test_name = malloc(128*sizeof(char));
  if (opt_sync)
    sprintf(test_name, "list-%s-%c", yield_str, sync_type);
  else
    sprintf(test_name, "list-%s-none", yield_str);
  int nops = nthreads * niters * 3;
  long long tot_time = nsec_time(ts_end) - nsec_time(ts_start);
  int time_per_op = tot_time / nops;
  char csv_output[256];
  sprintf(csv_output, "%s,%d,%d,%d,%d,%lld,%d\n",
	  test_name, nthreads, niters, 1, nops, tot_time, time_per_op);
  free(test_name);
  if (!opt_csv)
    printf("%s",csv_output);
  if (opt_csv) {
    int fd = open("lab2_list.csv", O_RDWR|O_APPEND|O_CREAT, 0666);
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
