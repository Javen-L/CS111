/*
NAME: Jianzhi Liu
EMAIL: ljzprivate@yahoo.com
ID: 204742214
SLIPDAYS: 0
*/
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mcrypt.h>

int DEBUG = 0;
struct termios orgn_tios;
MCRYPT td_en, td_de;
int encrypt_option = 0;

// modify the termios
void mod_termios(struct termios *tp) {
  (*tp).c_iflag = ISTRIP;
  (*tp).c_oflag = 0;
  (*tp).c_lflag = 0;
  tcsetattr(0, TCSANOW, tp);
}

// restore original termios
void restore_termios() {
  tcsetattr(0, TCSANOW, &orgn_tios);
  if (DEBUG) printf("{DEBUG} Terminal modes set back to original\n");
}

// stdin -> stdout: map <cr> or <lf> to <cr><lf>, replace INT&EOT
char *prepare_stdoutput(char *buf, int len) {
  char *new_buf = malloc(2*len*sizeof(char)+1);
  int i, j = 0;
  for ( i = 0; i < len; i++, j++) {
    if (buf[i] == '\r' || buf[i] == '\n') {
      new_buf[j] = '\r'; j++; new_buf[j] = '\n';
    }
    else if (buf[i] == '\x04') {
      new_buf[j] = '^'; j++; new_buf[j] = 'D';
      j++; break;
    }
    else if (buf[i] == '\x03') {
      new_buf[j] = '^'; j++; new_buf[j] = 'C';
    }
    else {
      new_buf[j] = buf[i];
    }
  }
  new_buf[j] = '\0';
  return new_buf;
}

// return 0 if nothing is read; return 0 on success
int handle_input(int ifd, int sockfd, int log_option, int logfd) {
  int bufsize = 256;
  char buf[bufsize+1];
  // read from ifd
  int rv_r = read(ifd, buf, bufsize);
  if (rv_r == -1) {
    restore_termios();
    fprintf(stderr, "Failed to read from fd0: %s\n", strerror(errno));
    exit(1);
  }
  if (!rv_r) return 0;
  buf[rv_r] = '\0';
  if (DEBUG) {
    if (encrypt_option && ifd == sockfd) printf("{DEBUG} from fd%d: [ENCRYPTED]\r\n", ifd);
    else printf("{DEBUG} from fd%d: [%s]\r\n", ifd, buf);
  }
  // input from stdin, send to stdout(fd1) after mapping and to server(sockfd) verbatimly
  if (ifd == 0) {
    // display on fd1
    char *buf1 = prepare_stdoutput(buf, rv_r);
    if (DEBUG) printf("{DEBUG} to fd1: [%s]\r\n", buf1);
    int rv_w1 = write(1, buf1, strlen(buf1));
    free(buf1);
    if (rv_w1 == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to fd1: %s\r\n", strerror(errno));
      exit(1);
    }
    // send to server
    if (DEBUG) printf("{DEBUG} to server(fd%d) (pre-encrypt): [%s]\r\n", sockfd, buf);
    if (encrypt_option) {
      mcrypt_generic(td_en, buf, rv_r);
      if (DEBUG) printf("{DEBUG} Input from client encrypted and sent\r\n");
    }
    int rv_ws = write(sockfd, buf, rv_r);
    if (rv_ws == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to server(fd%d): %s\r\n", sockfd, strerror(errno));
      exit(1);
    }
    if (log_option) {
      char log_msg[512];
      sprintf(log_msg, "SENT %d bytes: %s\n", (int) strlen(buf), buf);
      //if (DEBUG) printf("{DEBUG} Log: %s\r\n", log_msg);
      int rv_wlog = write(logfd, log_msg, strlen(log_msg));
      if (rv_wlog == -1) {
	restore_termios();
	fprintf(stderr, "Failed to write to log file(fd%d): %s\r\n", logfd, strerror(errno));
	exit(1);
      }
    }
  }
  // input from server, send to stdout(fd1) verbatimly
  else {
    if (log_option) {
      char log_msg[512];
      sprintf(log_msg, "RECEIVED %d bytes: %s\n", (int) strlen(buf), buf);
      //if (DEBUG) printf("{DEBUG} Log: %s\r\n", log_msg);
      int rv_wlog = write(logfd, log_msg, strlen(log_msg));
      if (rv_wlog == -1) {
        restore_termios();
        fprintf(stderr, "Failed to write to log file(fd%d): %s\n", logfd, strerror(errno));
        exit(1);
      }
    }
    if (encrypt_option) {
      mdecrypt_generic(td_de, buf, rv_r);
      if (DEBUG) printf("{DEBUG} Input from server decrypted\r\n");
    }
    if (DEBUG) printf("{DEBUG} to fd1: [%s]\r\n", buf);
    int rv_w1 = write(1, buf, rv_r);
    if (rv_w1 == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to fd1: %s\n", strerror(errno));
      exit(1);
    }
  }
  return 1;
}

void clt_exit(int exit_code) {
  if (encrypt_option) {
    mcrypt_generic_deinit(td_en); mcrypt_module_close(td_en);
    mcrypt_generic_deinit(td_de); mcrypt_module_close(td_de);
  }
  restore_termios();
  exit(exit_code);
}

int main(int argc, char *argv[]) {
  // parse options: --port --log
  int port_option = 0;
  char *port_num_str = NULL;
  int log_option = 0;
  char *log_filename = NULL;
  char *key_filename = NULL;
  char *usage = "lab1b-client --port=port# [--log=filename] [--debug]";
  static struct option opts[] = {
    {"port",    1, 0, 'p'},
    {"log",     1, 0, 'l'},
    {"debug",   0, 0, 'd'},
    {"encrypt", 1, 0, 'e'},
    {0, 0, 0, 0}
  };
  while (1) {
    int opt_index = -1;
    int rv = getopt_long(argc, argv, "", opts, &opt_index);
    if (rv == -1) break;
    else if (rv == '?') {
      fprintf(stderr, "Unrecognized argument. Usage: %s\n", usage);
      exit(1);
    }
    else if (rv == 'p') {
      port_option = 1;
      port_num_str = optarg;
      if (DEBUG) printf("{DEBUG} Port number: %s\n", port_num_str);
    }
    else if (rv == 'l') {
      log_option = 1;
      log_filename = optarg;
      if (DEBUG) printf("{DEBUG} Log filename: %s\n", log_filename);
    }
    else if (rv == 'e') {
      encrypt_option = 1;
      key_filename = optarg;
      if (DEBUG) printf("{DEBUG} Key filename: %s\n", key_filename);
    }
    else if (rv == 'd') {
      DEBUG = 1;
    }
    else {
      if (DEBUG) printf("{DEBUG} Unexpected error with parsing\n");
    }
  }
  if (!port_option) {
    fprintf(stderr, "Option --port is mandatory. Usage: %s\n", usage);
    exit(1);
  }

  // setup log file
  int log_fd = -1;
  if (log_option) {
    log_fd = creat(log_filename, 0666);
    if (log_fd < 0) {
      fprintf(stderr, "Cannot create log file %s: %s\n", log_filename, strerror(errno));
      exit(1);
    }
    if (DEBUG) printf("{DEBUG} Created log file %s\n", log_filename);
  }

  // read key file
  char keystr[256];
  int keylen = 0;
  if (encrypt_option) {
    int key_fd = open(key_filename, O_RDONLY);
    if (key_fd < 0) {
      fprintf(stderr, "Cannot open key file %s: %s\n", key_filename, strerror(errno));
      exit(1);
    }
    keylen = read(key_fd, keystr, 255);
    if (keylen < 0) {
      fprintf(stderr, "Cannot read key file %s: %s\n", key_filename, strerror(errno));
      exit(1);
    }
    keystr[keylen] = '\0';
    if (DEBUG) printf("{DEBUG} Key: %s\n", keystr);
  }

  // set up socket, connect to server
  int port_num = atoi(port_num_str);
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Failed to create a socket: %s\n", strerror(errno));
    exit(1);
  }
  struct hostent *server = gethostbyname("localhost");
  if (!server) {
    fprintf(stderr, "Failed to get host by name: %s\n", strerror(h_errno));
    exit(1);
  }
  struct sockaddr_in srv_addr;
  bzero((char *) &srv_addr, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&srv_addr.sin_addr.s_addr, server->h_length);
  srv_addr.sin_port = htons(port_num);
  if (connect(sockfd,(struct sockaddr *)&srv_addr,sizeof(srv_addr)) < 0) {
    fprintf(stderr, "Cannot connect to server: %s\n", strerror(errno));
    exit(1);
  }
  if (DEBUG) printf("{DEBUG} Connected to server via fd%d\n", sockfd);

  // store original terminal modes & change terminal to new modes
  tcgetattr(0, &orgn_tios);
  struct termios mod_tios;
  mod_tios = orgn_tios;
  mod_termios(&mod_tios);
  if (DEBUG) printf("{DEBUG} Changed terminal modes\r\n");

  // initialize input/output encryption
  if (encrypt_option) {
    td_en = mcrypt_module_open("twofish", NULL, "cfb", NULL);
    td_de = mcrypt_module_open("twofish", NULL, "cfb", NULL);
    if (td_en == MCRYPT_FAILED || td_de == MCRYPT_FAILED) {
      fprintf(stderr, "Failed to open encryption descriptor\r\n");
      exit(1);
    }
    int ivsize = mcrypt_enc_get_iv_size(td_en);
    char *IV = malloc(ivsize);
    int i, rv_en, rv_de;
    for (i = 0; i < ivsize; i++) {
      IV[i] = i;
    }
    rv_en = mcrypt_generic_init(td_en, keystr, keylen, IV);
    rv_de = mcrypt_generic_init(td_de, keystr, keylen, IV);
    if (rv_en < 0 || rv_de < 0) {
      fprintf(stderr, "Failed to initialize encryption\r\n");
      exit(1); 
    }
    if (DEBUG) printf("{DEBUG} Encryption initialized\r\n");
  }

  // set up a poll for reading from fd0 and reading from socket (server)
  struct pollfd fds[2];
  fds[0].fd = 0; // stdin
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[1].fd = sockfd; // socket (server)
  fds[1].events = POLLIN | POLLHUP | POLLERR;
  while(1) {
    int rv = poll(fds, 2, 0);
    if (rv == -1) {
      fprintf(stderr, "Failed to poll: %s\r\n", strerror(errno));
      clt_exit(1);
    }
    if (rv <= 0) continue;
    int i;
    for (i = 0; i < 2; i++) {
      if (fds[i].revents & POLLIN) {
	if (DEBUG) printf("{DEBUG} Ready to read from fd%d\r\n", fds[i].fd);
	int rv = handle_input(fds[i].fd, sockfd, log_option, log_fd);
	if (!rv) {
	  if (sockfd == fds[i].fd) {
	    if (DEBUG) printf("{DEBUG} 0 byte read from server(fd%d). Server finished. Exit\r\n", sockfd);
	    clt_exit(0);
	  }
	  if (close(fds[i].fd) == -1) {
	    fprintf(stderr, "Failed to close fd%d: %s\r\n", fds[i].fd, strerror(errno));
	    clt_exit(1);
	  }
	  if (DEBUG) printf("{DEBUG}: fd%d closed\r\n", fds[i].fd);
	}
      }
      if (fds[i].revents & POLLHUP) {
	if (DEBUG) printf("{DEBUG}: Hang-up from fd%d\r\n", fds[i].fd);
	if (i == 0) {
	  fprintf(stderr, "Hang-up from fd0\r\n");
	  clt_exit(1);
	}
	else clt_exit(0);
      }
      if (fds[i].revents & POLLERR) {
	if (DEBUG) printf("{DEBUG}: Error with fd%d\r\n", fds[i].fd);
	if (i == 0) {
	  fprintf(stderr, "Hang-up from fd0\r\n");
	  clt_exit(1);
	}
	else clt_exit(0);
      }
    }
  }
  return 0;
}
