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
pid_t cpid;
int sockfd = -1;
int newsockfd = -1;
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

void srv_exit(int exit_code) {
  if (encrypt_option) {
    mcrypt_generic_deinit(td_en); mcrypt_module_close(td_en);
    mcrypt_generic_deinit(td_de); mcrypt_module_close(td_de);
  }
  restore_termios();
  if (close(sockfd) == -1 || close(newsockfd) == -1) {
    fprintf(stderr, "Fail to close fd before exit: %s\n", strerror(errno));
    exit(1);
  }
  if (DEBUG) printf("{DEBUG} Sockfd, newsockfd closed\n");
  exit(exit_code);
}

// mapping code
// 2: map <cr> to <lf>, DO NOT replace INT&EOT         client -> shell
// 3: map <lf> to <cr><lf>, replace INT&EOT            shell -> client
char *prepare_output(char *buf, int len, int mapping_code) {
  char *new_buf = malloc(2*len*sizeof(char)+1);
  int i, j = 0;
  if (mapping_code == 2) {
    for ( i = 0; i < len; i++, j++) {
      if (buf[i] == '\r') {
        new_buf[j] = '\n';
      }
      else {
        new_buf[j] = buf[i];
	if (buf[i] == '\x04') { j++; break; }
      }
    }
  }
  else if (mapping_code == 3) {
    for ( i = 0; i < len; i++, j++) {
      if (buf[i] == '\n') {
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
  }
  new_buf[j] = '\0';
  return new_buf;
}

// find the index of the first EOT in buf, if no EOT return -1
int find_char(char ch, char *buf) {
  int i = 0;
  int index = -1;
  while (buf[i]) {
    if (buf[i] == ch) {
      index = i;
      break;
    }
    else i++;
  }
  return index;
}

// associate with fd1 the file originally linked with fd2
void change_fd(int fd1, int fd2) {
  if (close(fd1) == -1 ||
      dup(fd2)   == -1 ||
      close(fd2) == -1) {
    fprintf(stderr, "Fail to change fd\r\n");
    srv_exit(1);
  }
}

// graceful exit
void shut_down() {
  int sh_stat = 0;
  while(1) {
    int rv = waitpid(cpid, &sh_stat, WNOHANG | WUNTRACED | WCONTINUED);
    if (rv == -1) {
      srv_exit(1);
      break;
    }
    else if (rv == 0) sleep(1);
    else {
      fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", WTERMSIG(sh_stat), WEXITSTATUS(sh_stat));
      break;
    }
  }
  srv_exit(0);
}

void sig_handler(int sig) {
  if (DEBUG) printf("{DEBUG} Caught SIGPIPE%d\r\n", sig);
  if (sig == SIGPIPE) {
    shut_down();
  }
}

int handle_input (int ifd, int ofd_clt, int ofd_shell, pid_t cpid) {
  int bufsize = 256;
  char buf[bufsize+1]; 
  // read from ifd
  int rv_r = read(ifd, buf, bufsize);
  if (rv_r == -1) {
    fprintf(stderr, "Failed to read from fd%d: %s\r\n", ifd, strerror(errno));
    srv_exit(1);
  }
  if (rv_r == 0) return 0;

  buf[rv_r] = '\0';
  if (DEBUG) {
    if (encrypt_option && ifd == ofd_clt) printf("{DEBUG} from fd%d: [ENCRYPTED]\r\n", ifd);
    else printf("{DEBUG} from fd%d: [%s]\r\n", ifd, buf);
  }
  
  // types of communication:
  //     from    to      code
  //     stdin   stdout  1
  //     stdin   shell   2
  //     shell   stdout  3
  
  // input from client
  if (ifd == ofd_clt) {
    if (encrypt_option) {
      mdecrypt_generic(td_de, buf, rv_r);
      if (DEBUG) printf("{DEBUG} Input from client decrypted\r\n");
    }
    // look for EOT/interruption
    int INTindex = find_char('\x03', buf);
    int EOTindex = find_char('\x04', buf);
    // output to shell, code 2
    char *buf2 = prepare_output(buf, rv_r, 2);
    // write only contents before ^D
    if (buf2[0] == '\x04' || buf2[0] == '\x03') buf2[0] = '\0';
    if (EOTindex != -1) { buf2[EOTindex] = '\0'; }
    if (DEBUG) printf("{DEBUG} to sh [%s]\r\n", buf2);
    int rv_w2 = write(ofd_shell, buf2, strlen(buf2));
    free(buf2);
    if (rv_w2 == -1) {
      fprintf(stderr, "Failed to write to shell: %s\r\n", strerror(errno));
      srv_exit(1);
    }
    if (INTindex != -1) {
      if (DEBUG) printf("{DEBUG} From client(fd%d), encountered ^C. Send SIGINT to sh\r\n", ifd);
      if (kill(cpid, SIGINT) == -1) {
	fprintf(stderr, "Failed to send SIGINT to child process: %s\r\n", strerror(errno));
	srv_exit(1);
      }
    }
    if (EOTindex != -1) {
      if (DEBUG) printf("{DEBUG} From client(fd%d), encountered ^D. Turn off ofd to sh\r\n", ifd);
      if (close(ofd_shell) < 0) {
	fprintf(stderr, "Failed to close fd%d: %s\r\n", ofd_shell, strerror(errno));
	srv_exit(1);
      }
      if (DEBUG) printf("{DEBUG} fd%d closed\r\n", ofd_shell);
    }
  }
  // input from shell
  else {
    // look for EOT
    int EOTindex = find_char('\x04', buf);
    // output to client, code 3
    char *buf3 = prepare_output(buf, rv_r, 3);
    int len3 = strlen(buf3);
    if (DEBUG) printf("{DEBUG} to client(fd%d) (pre-encrypy): [%s]\r\n", ofd_clt, buf3);
    if (encrypt_option) {
      mcrypt_generic(td_en, buf3, len3);
      if (DEBUG) printf("{DEBUG} Input from shell encrypted and sent\r\n");
    }
    int rv_w3 = write(ofd_clt, buf3, len3);
    free(buf3);
    if (rv_w3 == -1) {
      fprintf(stderr, "Failed to write to client(fd%d): %s\r\n", ofd_clt, strerror(errno));
      srv_exit(1);
    }
    if (EOTindex != -1) {
      if (DEBUG) printf("{DEBUG} From sh(fd%d), encountered ^D. Shut down\r\n", ifd);
      shut_down();
    }
  }
  return 1;
}

int main(int argc, char *argv[]) {
  // parse options: --port
  int port_option = 0;
  char *port_num_str = NULL;
  char *key_filename = NULL;
  char *usage = "lab1a [--shell=program] [--debug]";
  static struct option opts[] = {
    {"port",    1, 0, 'p'},
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

  // set up socket, bind, and wait for connection
  int port_num = atoi(port_num_str);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Failed to create a socket: %s\n", strerror(errno));
    exit(1);
  }
  struct sockaddr_in serv_addr, cli_addr;
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_num);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Failed to bind server to port: %s\n", strerror(errno));
    close(sockfd);
    exit(1);
  }
  listen(sockfd,5);
  socklen_t clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0) {
    fprintf(stderr, "Failed to accept: %s\n", strerror(errno));
    close(sockfd);
    exit(1);
  }
  if (DEBUG) printf("{DEBUG} Connected with client via fd%d\n", newsockfd);  

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
      fprintf(stderr, "Failed to initialize encryption\n");
      exit(1);
    }
    if (DEBUG) printf("{DEBUG} Encryption initialized\r\n");
  }

  // setup pipes & fork
  int t2s_pipefd[2];
  int s2t_pipefd[2];
  if (pipe(t2s_pipefd) == -1 || pipe(s2t_pipefd) == -1) {
    fprintf(stderr, "Failed to pipe: %s\r\n", strerror(errno));
    srv_exit(1);
  }
  if (DEBUG) printf("{DEBUG} Pipes set up\r\n");
  cpid = fork();
  if (cpid == -1) {
    fprintf(stderr, "Failed to fork: %s\r\n", strerror(errno));
    srv_exit(1);
  }
  else if (cpid == 0) { // child process, shell
    if (DEBUG) printf("{DEBUG} Child process %d started\r\n", getpid());
    // configure pipes
    // terminal to shell, shell does not write
    if (close(t2s_pipefd[1]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", t2s_pipefd[1], strerror(errno));
      restore_termios();
      _exit(1);
    }
    // shell to terminal, shell does not rea
    if (close(s2t_pipefd[0]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", s2t_pipefd[0], strerror(errno));
      restore_termios();
      _exit(1);
    }
    change_fd(0, t2s_pipefd[0]); // associate the fd0 of shell with t2s_pipe
    change_fd(1, s2t_pipefd[1]); // associate the fd1 of shell with s2t_pipe
    close(2); dup(1); // fd2 and fd1 share the same s2t_pipe
    if (close(2) == -1 || dup(1) == -1) {
      fprintf(stderr, "Failed to close fd2 and dup fd1: %s\r\n", strerror(errno));
      restore_termios();
      _exit(1);
    }
    char *path = "/bin/bash";
    if (execl(path, path, (char *) NULL) < 0) {
      fprintf(stderr, "Cannot execute %s\r\n", path);
      restore_termios();
      _exit(1);
    }
  }
  else { // parent process, terminal
    // configure pipes
    // terminal to shell, terminal does not read
    if (close(t2s_pipefd[0]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", t2s_pipefd[0], strerror(errno));
      srv_exit(1);
    }
    // shell to terminal, terminal does not write
    if (close(s2t_pipefd[1]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", s2t_pipefd[1], strerror(errno));
      srv_exit(1);
    }

    // setup poll
    struct pollfd fds[2];
    fds[0].fd = newsockfd; // fd for client(newsockfd)
    fds[0].events = POLLIN | POLLHUP | POLLERR;
    fds[1].fd = s2t_pipefd[0]; // fd for shell to terminal pipe
    fds[1].events = POLLIN | POLLHUP | POLLERR;

    // register signal handler for the exit of shell
    signal(SIGPIPE, sig_handler);

    // wait for events from poll
    while (1) {
      int rv = poll(fds, 2, 0);
      if (rv == -1) {
	fprintf(stderr, "Failed to poll: %s\r\n", strerror(errno));
	srv_exit(1);
      }
      if (rv <= 0) continue;
      int i;
      for (i = 0; i < 2; i++) {
	if (fds[i].revents & POLLIN) {
	  if (DEBUG) printf("{DEBUG} Ready to read from fd%d\r\n", fds[i].fd);
	  int rv = handle_input(fds[i].fd, newsockfd, t2s_pipefd[1], cpid);
	  if (!rv) {
	    if (newsockfd == fds[i].fd) {
	      if (DEBUG) printf("{DEBUG} 0 byte read from client(fd%d). Client finished. Shut down\r\n", newsockfd);
	      shut_down();
	    }
	    if (close(fds[i].fd) == -1) {
	      fprintf(stderr, "Failed to close fd%d: %s\r\n", fds[i].fd, strerror(errno));
	      srv_exit(1);
	    }
	    if (DEBUG) printf("{DEBUG} 0 byte read from fd%d, closed\r\n", fds[i].fd);
	  }
	}
	if (fds[i].revents & POLLHUP) {
	  if (DEBUG) printf("{DEBUG} Hang-up from fd%d\r\n", fds[i].fd);
	  shut_down();
	}
	if (fds[i].revents & POLLERR) {
	  if (DEBUG) printf("{DEBUG} Error with fd%d\r\n", fds[i].fd);
	  shut_down();
	}
      }
    } 
  }
  return 0;
}
