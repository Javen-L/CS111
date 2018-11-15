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

struct sublist {
  pthread_mutex_t mutex;
  int slock;
  SortedList_t *list;
};
typedef struct sublist sublist_t;
sublist_t *sublists;
SortedListElement_t *elements = NULL;
int nelems = 0;

static int nthreads = 1;
static int niters = 1;
static int nlists = 1;
int opt_csv = 0;
int opt_yield = 0;
char *yield_str = NULL;
int opt_sync = 0;
char sync_type = 0;
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//static int slock = 0;

long long wait_time[64] = {0};
int nlocks[64] = {0};

int hash_func(const char *key) {
  long sum = 0;
  int len = strlen(key);
  int i;
  for (i = 0; i < len; i++) {
    sum += key[i];
  }
  return sum % nlists;
}

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
  int i;
  for (i = 0; i < nlists; i++) {
    free(sublists[i].list);
  }
  free(sublists);
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
void printls(SortedList_t *list) {
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

void update_wait_time(int index, struct timespec start, struct timespec end) {
  long long lapse = nsec_time(end) - nsec_time(start);
  wait_time[index] += lapse;
  nlocks[index]++;
  if (DEBUG) printf("[DEBUG] Wait_time of thread %d: %d locks, %lldns\n", index, nlocks[index], wait_time[index]);
}

long long avg_wait_time() {
  long long tot_time = 0;
  int tot_nlocks = 0;
  int i;
  for (i = 0; i < nthreads; i++) {
    tot_time += wait_time[i];
    tot_nlocks += nlocks[i];
  }
  if (DEBUG) printf("[DEBUG] total wait time: %lldns, total lock number: %d", tot_time, tot_nlocks);
  if (!tot_nlocks) return 0;
  else return tot_time/tot_nlocks;
}

void *thread_func(void *p) {
  struct timespec wait_start, wait_end;
  int index = *((int *) p);
  int offset = index * niters;
  int lmt = offset + niters;
  int i;
  if (DEBUG) printf("[DEBUG] Thread %d got elements[%d:%d]\n", *((int *)p), offset, lmt);
  // insert
  for (i = offset; i < lmt; i++) {
    int k = hash_func(elements[i].key);
    SortedList_t *list = sublists[k].list;
    pthread_mutex_t *mutex = &(sublists[k].mutex);
    int *slock = &(sublists[k].slock);
    if (!opt_sync) {
      SortedList_insert(list, &elements[i]);
    }
    else if (sync_type == 'm') {
      if (clock_gettime(CLOCK_MONOTONIC, &wait_start) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      pthread_mutex_lock(mutex);
      if (clock_gettime(CLOCK_MONOTONIC, &wait_end) == -1) {
        fprintf(stderr, "Failed to get time from clock\n");
        exit(1);
      }
      update_wait_time(index, wait_start, wait_end);
      SortedList_insert(list, &elements[i]);
      pthread_mutex_unlock(mutex);
    }
    else if (sync_type == 's') {
      if (clock_gettime(CLOCK_MONOTONIC, &wait_start) == -1) {
        fprintf(stderr, "Failed to get time from clock\n");
        exit(1);
      }
      while(__sync_lock_test_and_set(slock,1))
	; // spin
      if (clock_gettime(CLOCK_MONOTONIC, &wait_end) == -1) {
        fprintf(stderr, "Failed to get time from clock\n");
        exit(1);
      }
      update_wait_time(index, wait_start, wait_end);
      SortedList_insert(list, &elements[i]);
      __sync_lock_release(slock);
    }
  }
  // get length
  int len = 0;
  for (i = 0; i < nlists; i++) {
    SortedList_t *list = sublists[i].list;
    pthread_mutex_t *mutex = &(sublists[i].mutex);
    int *slock = &(sublists[i].slock);
    if (!opt_sync) {
      int sub_len = SortedList_length(list);
      if (sub_len == -1) {
	fprintf(stderr, "Something went wrong with list insert/length\n");
	exit(2);
      }
      len += sub_len;
    }
    else if (sync_type == 'm') {
      if (clock_gettime(CLOCK_MONOTONIC, &wait_start) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      pthread_mutex_lock(mutex);
      if (clock_gettime(CLOCK_MONOTONIC, &wait_end) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      update_wait_time(index, wait_start, wait_end);
      int sub_len = SortedList_length(list);
      if (sub_len == -1) {
        fprintf(stderr, "Something went wrong with list insert/length\n");
        exit(2);
      }
      len += sub_len;
      pthread_mutex_unlock(mutex);
    }
    else if (sync_type == 's') {
      if (clock_gettime(CLOCK_MONOTONIC, &wait_start) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      while(__sync_lock_test_and_set(slock,1))
	; // spin
      if (clock_gettime(CLOCK_MONOTONIC, &wait_end) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      update_wait_time(index, wait_start, wait_end);
      int sub_len = SortedList_length(list);
      if (sub_len == -1) {
        fprintf(stderr, "Something went wrong with list insert/length\n");
        exit(2);
      }
      len += sub_len;
      __sync_lock_release(slock);
    }
  }
  
  // look up & delete
  for (i = offset; i < lmt; i++) {
    int k = hash_func(elements[i].key);
    SortedList_t *list = sublists[k].list;
    pthread_mutex_t *mutex = &(sublists[k].mutex);
    int *slock = &(sublists[k].slock);
    if (!opt_sync) {
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
    }
    else if (sync_type == 'm') {
      if (clock_gettime(CLOCK_MONOTONIC, &wait_start) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      pthread_mutex_lock(mutex);
      if (clock_gettime(CLOCK_MONOTONIC, &wait_end) == -1) {
	fprintf(stderr, "Failed to get time from clock\n");
	exit(1);
      }
      update_wait_time(index, wait_start, wait_end);
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
      pthread_mutex_unlock(mutex);
    }
    else if (sync_type == 's') {
      if (clock_gettime(CLOCK_MONOTONIC, &wait_start) == -1) {
        fprintf(stderr, "Failed to get time from clock\n");
        exit(1);
      }
      while(__sync_lock_test_and_set(slock,1))
	; // spin
      if (clock_gettime(CLOCK_MONOTONIC, &wait_end) == -1) {
        fprintf(stderr, "Failed to get time from clock\n");
        exit(1);
      }
      update_wait_time(index, wait_start, wait_end);
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
      __sync_lock_release(slock);
    }
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
    {"lists",      1, 0, 'l'},
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
    case 'l':
      nlists = atoi(optarg);
      if (DEBUG) printf("[DEBUG] # of sublists set to %c\n", nlists);
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
  sublists = (sublist_t *) malloc(nlists*sizeof(sublist_t));
  if (!sublists) {
    malloc_error();
  }
  int i;
  for (i = 0; i < nlists; i++) {
    pthread_mutex_init(&(sublists[i].mutex), NULL);
    sublists[i].slock = 0;
    sublists[i].list = (SortedList_t *) malloc(sizeof(SortedList_t));
    sublists[i].list->key = NULL;
    sublists[i].list->next = NULL;
    sublists[i].list->prev = NULL;
  }
  if (DEBUG) printf("[DEBUG] Empty sublists initialized\n");
  // create and initialize threads*iters number of list elements, with random keys
  nelems = nthreads*niters;
  elements = (SortedListElement_t*) malloc(nelems*sizeof(SortedListElement_t));
  if (!elements) {
    free(sublists);
    malloc_error();
  }
  for (i = 0; i < nelems; i++) {
    elements[i].key = rand_key();
    elements[i].prev = NULL;
    elements[i].next = NULL;
  }
  if (DEBUG) printf("[DEBUG] Elements initialized with random keys\n");
  // signal segfault
  signal(SIGSEGV, sighandler);
  // record start time
  struct timespec ts_start;
  if (clock_gettime(CLOCK_MONOTONIC, &ts_start) == -1) {
    fprintf(stderr, "Failed to get time from clock\n");
    free_list();
    exit(1);
  }
  // set up threads
  pthread_t *threads = (pthread_t *) malloc(nthreads*sizeof(pthread_t));
  if (!threads) {
    fprintf(stderr, "Failed to malloc for threads\n");
    exit(1);
  }
  int thread_args[nthreads];
  for (i = 0; i < nthreads; i++) {
    thread_args[i] = i;
  }
  for (i = 0; i < nthreads; i++) {
    if (DEBUG) printf("[DEBUG] thread %d created\n",i);
    if (pthread_create(&threads[i], NULL, thread_func, (void *) &thread_args[i])) {
      fprintf(stderr, "Failed to create threads: %s\n", strerror(errno));
      free(threads);
      free_list();
      exit(1);
    }
  }
  // wait for threads
  for (i = 0; i < nthreads; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "Failed to join threads: %s\n", strerror(errno));
      free(threads);
      free_list();
      exit(1);
    }
  }
  free(threads);
  // record end time
  struct timespec ts_end;
  if (clock_gettime(CLOCK_MONOTONIC, &ts_end) == -1) {
    fprintf(stderr, "Failed to get time from clock\n");
    free_list();
    exit(1);
  }
  // check final list
  for (i = 0; i < nlists; i++) {
    int sub_len = SortedList_length(sublists[i].list);
    if (sub_len) {
      fprintf(stderr, "Final length of the list is none-zero: %d\n", sub_len);
      free_list();
      exit(2);
    }
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
  int wait_per_lock = avg_wait_time();
  char csv_output[256];
  sprintf(csv_output, "%s,%d,%d,%d,%d,%lld,%d,%d\n",
	  test_name, nthreads, niters, nlists, nops, tot_time, time_per_op, wait_per_lock);
  free(test_name);
  if (!opt_csv)
    printf("%s",csv_output);
  if (opt_csv) {
    int fd = open("lab2b_list.csv", O_RDWR|O_APPEND|O_CREAT, 0666);
    if (fd == -1) {
      fprintf(stderr, "Failed to open csv file: %s\n", strerror(errno));
      exit(1);
    }
    if (write(fd, csv_output, strlen(csv_output)) == -1) {
      fprintf(stderr, "Failed to write to csv file: %s\n", strerror(errno));
      exit(1);
    }
  }
  return 0;
}
