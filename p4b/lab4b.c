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
#include <poll.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <mraa/gpio.h>
#include <mraa.h>
#include <math.h>

char DEBUG = 0;

double period = 1;
int CorF = 'F';
int log_option = 0;
char *log_filename = NULL;
int logfd = -1;
int running = 1;
int shutdown_cond = 0;
struct timeval prev_tv;
struct timezone tz;

int bufsize = 256;
char input_buf[512];
int offset = 0;

char cmds[7][16] = {
  "SCALE=F",
  "SCALE=C",
  "STOP",
  "START",
  "OFF",
  "PERIOD=",
  "LOG"
};

mraa_gpio_context button;
mraa_aio_context temp_sensor;

// convert from C to F
double C2F(double c) {
  return c * 9.0 / 5.0 + 32;
}

// return a double representing temperature in F/C
double read_temperature() {
  double temp = 0;
  int B = 4275;
  int R0 = 100000;
  int a = mraa_aio_read(temp_sensor);
  double R = 1023.0/a - 1.0;
  R *= R0;
  double temp_c = 1.0/(log(R/R0)/B+1/298.15)-273.15;
  if (CorF == 'F') temp = C2F(temp_c);
  else temp = temp_c;
  return temp;
}

void shutdown(int exitcode) {
  mraa_gpio_close(button);
  mraa_aio_close(temp_sensor);
  exit(exitcode);
}

// return a pointer to the time string on heap
char *get_time() {
  time_t rawtime;
  struct tm *t;
  time(&rawtime);
  t = localtime(&rawtime);
  char *time_str = malloc(32*sizeof(char));
  if (!time_str) {
    fprintf(stderr, "Failed to allocate memory\n");
    shutdown(1);
  }
  sprintf(time_str, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  return time_str;
}

// find the number of seconds that passed between two tv's
double lapse(struct timeval prev_tv, struct timeval tv) {
  double diff, diff_s, diff_ms = 0;
  diff_s = tv.tv_sec - prev_tv.tv_sec;
  diff_ms = tv.tv_usec - prev_tv.tv_usec;
  diff = diff_s + diff_ms / 1000000;
  if (DEBUG && diff >= period) printf("[DEBUG] lapse = %0.2f\n", diff);
  return diff;
}

// return 1 if there is no '\n' in str && str ends with '\0'
// return 0 otherwise
int valid_residue(char *str, int size) {
  int rv = 1;
  int i;
  for (i = 0; i < size; i++) {
    if (str[i] == '\n') {rv = 0; break;}
  }
  if (str[size] != '\0') rv = 0;
  return rv;
}

void output_n_log() {
  char *output = malloc(128*sizeof(char));
  if (!output) {
    fprintf(stderr, "Failed to allocate memory\n");
    shutdown(1);
  }
  char *time_str = get_time();
  if (!shutdown_cond) {
    double temp = read_temperature();
    sprintf(output, "%s %0.1f\n", time_str, temp);
  }
  else {
    sprintf(output, "%s SHUTDOWN\n", time_str);
  }
  free(time_str);
  printf("%s", output);
  if (log_option) {
    if (write(logfd, output, strlen(output)) == -1) {
      fprintf(stderr, "Cannot write to log %s: %s\n", log_filename, strerror(errno));
      shutdown(1);
    }
  }
  free(output);
  if (shutdown_cond) {
    shutdown(0);
  }
}

void shutdown_sighandler() {
  shutdown_cond = 1;
  output_n_log();
}

int handle_command() {
  if (!valid_residue(input_buf, offset)) {
    fprintf(stderr, "Something went wrong with command parsing\nResidue: {%s}\n", input_buf);
    shutdown(1);
  }
  // read from stdin
  char *eob  = input_buf + offset;
  int readrv = read(0, eob, bufsize - offset);
  if (readrv == -1) {
    fprintf(stderr, "Failed to read from fd0: %s\n", strerror(errno));
    shutdown(1);
  }
  if (readrv == 0) return 0;
  eob[readrv] = '\0';
  offset += readrv;
  if (DEBUG) printf("[DEBUG] Newly read from stdin: {%s}\n", eob);
  if (DEBUG) printf("[DEBUG] Buffer content: {%s}\n", input_buf);
  // try to find command in buffer
  int i;
  while(!valid_residue(input_buf, offset)) {
    // find location of next '\n'
    for (i = 0; i < offset; i++) {
      if (input_buf[i] == '\n') break;
    }
    // copy characters before '\n' to a buffer
    char cmd_buf[512];
    memcpy(cmd_buf, input_buf, i);
    cmd_buf[i] = '\0';
    // remove contents before '\n'
    i++;
    strcpy(input_buf, &input_buf[i]);
    offset -= i;
    // parsing characters in buffer
    int match = 0;
    int j;
    for (j = 0; j < 5; j++) {
      if (!strcmp((const char *)cmd_buf, cmds[j])) {
	char OFF[4] = "OFF\n";
	switch (j) {
	case 0: // SCALE = F
	  CorF = 'F'; break;
	case 1: // SCALE = C
	  CorF = 'C'; break;
	case 2: // STOP
	  running = 0; break;
	case 3: // START
	  running = 1; break;
	case 4: // OFF
	  if (log_option) {
	    if (write(logfd, OFF, 4) == -1) {
	      fprintf(stderr, "Cannot write to log %s: %s\n", log_filename, strerror(errno));
	      shutdown(1);
	    }
	  }
	  shutdown_sighandler();
	  break;
	}
	match = 1;
	if (DEBUG) printf("[DEBUG] Command found: {%s}\n", cmd_buf);
	break;
      }
    }
    // handle period and log command
    if (!match && !memcmp((const char *)cmd_buf, cmds[5], 7)) { // try matching "PERIOD="
      double new_period = atof(&cmd_buf[7]);
      if (new_period) {
	period = new_period;
	if (DEBUG) printf("[DEBUG] Period updated to %0.2f\n", period);
	match = 1;
      }
    }
    if (!match && !memcmp((const char *)cmd_buf, cmds[6], 3)) { // try matching "LOG"
      if (DEBUG) printf("[DEBUG] Log: {%s}\n", &cmd_buf[3]);
      match = 1;
    }
    cmd_buf[i-1] = '\n';
    cmd_buf[i] = '\0';
    if (DEBUG) printf("[DEBUG] Logging {%s}\n", cmd_buf);
    if (match && log_option) {
      if (write(logfd, cmd_buf, strlen(cmd_buf)) == -1) {
	fprintf(stderr, "Cannot write to log %s: %s\n", log_filename, strerror(errno));
	shutdown(1);
      }
    }
    if (DEBUG && !match) printf("[DEBUG] Input ignored: {%s}\n", cmd_buf);
  }
  return 1;
}

int main(int argc, char *argv[]) {
  if (gettimeofday(&prev_tv, &tz) == -1) {
    fprintf(stderr, "Failed to gettimeofday: %s\n", strerror(errno));
    exit(1);
  }
  // parsing
  char *usage = "lab4b [--period=#] [--scale=C|F] [--log=filename]";
  static struct option opts[] = {
    {"period", 1, 0, 'p'},
    {"scale",  1, 0, 's'},
    {"log",    1, 0, 'l'},
    {"debug",  0, 0, 'd'},
    {0, 0, 0, 0}
  };
  while (1) {
    int opt_index = -1;
    int rv = getopt_long(argc, argv, "", opts, &opt_index);
    if (rv == -1) break;
    switch (rv) {
    case '?':
      fprintf(stderr, "Argument unrecognized\nUsage: %s\n",usage);
      exit(1);
      break;
    case 'p':
      period = atof(optarg);
      if (DEBUG) printf("[DEBUG] Period set to %0.1fs\n", period);
      break;
    case 's':
      CorF = optarg[0];
      if (CorF != 'C' && CorF != 'F') {
	fprintf(stderr, "Scale can only be C or F\n");
	exit(1);
      }
      if (DEBUG) printf("[DEBUG] Scale set to %c\n", CorF);
      break;
    case 'l':
      log_option = 1;
      log_filename = optarg;
      if (DEBUG) printf("[DEBUG] logfile set to %s\n", log_filename);
      break;
    case 'd':
      DEBUG = 1;
      break;
    default:
      fprintf(stderr, "Unexpected error in parsing.\nReturn value: %c\n",rv);
      exit(1);
    }
  }
  // set up logfile
  if (log_option) {
    logfd = open(log_filename, O_RDWR|O_APPEND|O_CREAT, 0666);
    if (logfd == -1) {
      fprintf(stderr, "Failed to open log %s: %s\n", log_filename, strerror(errno));
      exit(1);
    }
  }
  if (DEBUG && log_option) {
    char test[13] = "[DEBUG] TEST\n";
    if (write(logfd, test, 13) == -1) {
      fprintf(stderr, "Cannot write to log %s: %s\n", log_filename, strerror(errno));
      exit(1);
    }
  }
  // set up devices
  // button
  button = mraa_gpio_init(60);
  if (!button) {
    fprintf(stderr, "Failed to initialize GPIO\n");
    exit(1);
  }
  mraa_gpio_dir(button, MRAA_GPIO_IN);
  mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &shutdown_sighandler, NULL);
  // temperature sensor
  temp_sensor = mraa_aio_init(1);
  if (!temp_sensor) {
    fprintf(stderr, "Failed to initialize AIO\n");
    mraa_gpio_close(button);
    exit(1);
  }
  //-------------------after this line, closure needed before exit-----------------
  // set up poll
  struct pollfd fds[1];
  fds[0].fd = 0;
  fds[0].events = POLLIN;

  // keep reading temperatures from sensor
  int first_time = 1;
  while (1) {
    struct timeval tv;
    if (running && gettimeofday(&tv, &tz) == -1) {
      fprintf(stderr, "Failed to gettimeofday: %s\n", strerror(errno));
      shutdown(1);
    }
    if (first_time || (running && lapse(prev_tv, tv) >= period)) {
      first_time = 0;
      prev_tv = tv;
      output_n_log();
    }
    int pollrv = poll(fds, 1, 0);
    if (pollrv == -1) {
      fprintf(stderr, "Failed to poll: %s\n", strerror(errno));
      shutdown(1);
    }
    if (pollrv == 0) continue; // no input ready
    if (fds[0].revents & POLLIN) {
      int rv = handle_command();
      if (!rv) {
	//if (DEBUG) printf("POLLIN nothing from stdin\n");
	//break;
      }
    }
  }
  return 0;
}
