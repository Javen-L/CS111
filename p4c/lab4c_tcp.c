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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <mraa/gpio.h>
#include <mraa.h>
#include <math.h>

char DEBUG = 0;

double period = 1;
int CorF = 'F';
char *id = NULL;
char *hostname = NULL;
char *log_filename = NULL;
int port = -1;
int logfd = -1;
int sockfd = -1;
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

void shut_down(int exitcode) {
  mraa_aio_close(temp_sensor);
  if (logfd != -1) close(logfd);
  if (sockfd != -1) close(sockfd);
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
    shut_down(2);
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

// write message to ofd, shutdown when failure
void outputmsg(int ofd, char *message) {
  int len =  strlen(message);
  int rv_w = write(ofd, message, len);
  if (rv_w == -1) {
    fprintf(stderr, "Failed to write to fd%d: %s\r\n", ofd, strerror(errno));
    shut_down(2);
  }
}

void sample_n_output() {
  char *output = malloc(128*sizeof(char));
  if (!output) {
    fprintf(stderr, "Failed to allocate memory\n");
    shut_down(2);
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
  //printf("%s", output);
  if (DEBUG) {
    char *debug_output = malloc(128*sizeof(char));
    sprintf(debug_output, "[DEBUG] %s", output);
    outputmsg(1, debug_output);
    free(debug_output);
  }
  outputmsg(sockfd, output);
  outputmsg(logfd, output);
  free(output);
  if (shutdown_cond) {
    shut_down(0);
  }
}

int handle_command() {
  if (!valid_residue(input_buf, offset)) {
    fprintf(stderr, "Something went wrong with command parsing\nResidue: {%s}\n", input_buf);
    shut_down(2);
  }
  // read from sockfd
  char *eob  = input_buf + offset;
  int readrv = read(sockfd, eob, bufsize - offset);
  if (readrv == -1) {
    fprintf(stderr, "Failed to read from sockfd: %s\n", strerror(errno));
    shut_down(2);
  }
  if (readrv == 0) return 0;
  eob[readrv] = '\0';
  offset += readrv;
  if (DEBUG) printf("[DEBUG] Newly read from server: {%s}\n", eob);
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
	  outputmsg(logfd, OFF);
	  shutdown_cond = 1;
	  sample_n_output();
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
    if (match) {
      outputmsg(logfd, cmd_buf);
    }
    if (DEBUG && !match) printf("[DEBUG] Input ignored: {%s}\n", cmd_buf);
  }
  return 1;
}

// return 1 if id is a 9-digit-number; return 0 otherwise
int valid_id(char *id) {
  if (!id) return 0;
  if (strlen(id) != 9) return 0;
  int i;
  for (i = 0; i < 9; i++) {
    if (id[i] < '0' || id[i] > '9') return 0;
  }
  return 1;
}

// set up socket and connect to server
void set_up_connection() {
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Failed to create a socket: %s\n", strerror(errno));
    shut_down(2);
  }
  struct hostent *server = gethostbyname(hostname);
  if (!server) {
    fprintf(stderr, "Failed to get host by name: %s\n", strerror(h_errno));
    shut_down(1);
  }
  struct sockaddr_in srv_addr;
  bzero((char *) &srv_addr, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&srv_addr.sin_addr.s_addr, server->h_length);
  srv_addr.sin_port = htons(port);
  if (connect(sockfd,(struct sockaddr *)&srv_addr,sizeof(srv_addr)) < 0) {
    fprintf(stderr, "Cannot connect to server: %s\n", strerror(errno));
    shut_down(2);
  }
  if (DEBUG) printf("{DEBUG} Connected to server via fd%d\n", sockfd);
  char msg[12];
  sprintf(msg, "ID=%s\n", id);
  if (DEBUG) {
    char *debug_output = malloc(128*sizeof(char));
    sprintf(debug_output, "[DEBUG] %s", msg);
    outputmsg(1, debug_output);
    free(debug_output);
  }
  outputmsg(sockfd, msg);
  outputmsg(logfd, msg);
}

int main(int argc, char *argv[]) {
  if (gettimeofday(&prev_tv, &tz) == -1) {
    fprintf(stderr, "Failed to gettimeofday: %s\n", strerror(errno));
    exit(1);
  }
  // parsing
  char *usage = "lab4c_tcp [--period=#] [--scale=C|F] --id=9-digit-number --host=name/addr --log=filename";
  static struct option opts[] = {
    {"period", 1, 0, 'p'},
    {"scale",  1, 0, 's'},
    {"id",     1, 0, 'i'},
    {"host",   1, 0, 'h'},
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
    case 'i':
      id = optarg;
      if (DEBUG) printf("[DEBUG] id set to %s\n", id);
      break;
    case 'h':
      hostname = optarg;
      if (DEBUG) printf("[DEBUG] hostname set to %s\n", hostname);
      break;
    case 'l':
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
  if (optind < argc)
    port = atoi(argv[optind]);
  if (DEBUG) printf("[DEBUG] port set to %d\n", port);
  // check if all required arguments are provided
  if (!(valid_id(id) && hostname && log_filename && port != -1)) {
      fprintf(stderr, "Required argument(s) incorrect\nUsage: %s\n",usage);
      exit(1);
  }
  // set up logfile
  logfd = open(log_filename, O_RDWR|O_APPEND|O_CREAT, 0666);
  if (logfd == -1) {
    fprintf(stderr, "Failed to open log %s: %s\n", log_filename, strerror(errno));
    exit(1);
  }
  if (DEBUG) {
    char test[13] = "[DEBUG] TEST\n";
    if (write(logfd, test, 13) == -1) {
      fprintf(stderr, "Cannot write to log %s: %s\n", log_filename, strerror(errno));
      exit(1);
    }
  }
  // set up devices
  // temperature sensor
  temp_sensor = mraa_aio_init(1);
  if (!temp_sensor) {
    fprintf(stderr, "Failed to initialize AIO\n");
    exit(1);
  }
  //-------------------after this line, closure needed before exit-----------------
  // set up socket and connect to server
  set_up_connection();
  // set up poll
  struct pollfd fds[1];
  fds[0].fd = sockfd;
  fds[0].events = POLLIN;

  // keep reading temperatures from sensor
  int first_time = 1;
  while (1) {
    struct timeval tv;
    if (running && gettimeofday(&tv, &tz) == -1) {
      fprintf(stderr, "Failed to gettimeofday: %s\n", strerror(errno));
      shut_down(2);
    }
    if (first_time || (running && lapse(prev_tv, tv) >= period)) {
      first_time = 0;
      prev_tv = tv;
      sample_n_output();
    }
    int pollrv = poll(fds, 1, 0);
    if (pollrv == -1) {
      fprintf(stderr, "Failed to poll: %s\n", strerror(errno));
      shut_down(2);
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
